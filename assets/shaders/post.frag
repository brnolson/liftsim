#version 460 core
// Post-process: sky, ink outlines, aerial fog, ACES tone mapping, gamma.

in vec2 vUV;
out vec4 fragColor;

uniform sampler2D uSceneColor;
uniform sampler2D uSceneNormal;
uniform sampler2D uSceneDepth;
uniform vec2  uTexel;
uniform float uNear;
uniform float uFar;
uniform mat4  uInvViewProj;
uniform vec3  uSunDir;
uniform int   uOutlinesOn;

// Keep in sync with SkyColor() in scene.frag.
vec3 SkyColor(vec3 dir) {
    float height = clamp(dir.y, 0.0, 1.0);
    vec3 haze = vec3(0.62, 0.72, 0.88);
    vec3 sky = mix(haze, vec3(0.16, 0.34, 0.72), pow(height, 0.5));
    // Below the horizon the view only sees distant haze, so darken the horizon
    // colour gently instead of switching to a ground colour (which shows as a seam).
    sky = mix(haze * 0.85, sky, smoothstep(-0.3, 0.0, dir.y));
    float sun = pow(max(dot(dir, -uSunDir), 0.0), 900.0);
    return sky + sun * vec3(20.0, 18.0, 15.0);
}

// Perspective depth buffer value (0..1) back to eye-space distance.
float LinearDepth(float d) {
    float z = d * 2.0 - 1.0;
    return 2.0 * uNear * uFar / (uFar + uNear - z * (uFar - uNear));
}

vec3 ViewRay(vec2 uv) {
    vec4 nearP = uInvViewProj * vec4(uv * 2.0 - 1.0, -1.0, 1.0);
    vec4 farP = uInvViewProj * vec4(uv * 2.0 - 1.0, 1.0, 1.0);
    return normalize(farP.xyz / farP.w - nearP.xyz / nearP.w);
}

// Edges are where depth jumps (silhouettes) or normals turn sharply (creases).
float EdgeStrength(vec2 uv, float depthCenter) {
    vec3 normalCenter = texture(uSceneNormal, uv).xyz * 2.0 - 1.0;
    const vec2 offsets[4] = vec2[](vec2(1, 0), vec2(-1, 0), vec2(0, 1), vec2(0, -1));
    float edge = 0.0;
    for (int i = 0; i < 4; ++i) {
        vec2 sampleUV = uv + offsets[i] * uTexel;
        float depthSample = LinearDepth(texture(uSceneDepth, sampleUV).r);
        float depthEdge = smoothstep(0.04, 0.10, abs(depthCenter - depthSample) / depthCenter);

        vec3 normalSample = texture(uSceneNormal, sampleUV).xyz * 2.0 - 1.0;
        float normalEdge = smoothstep(0.25, 0.6, 1.0 - dot(normalCenter, normalSample));
        edge = max(edge, max(depthEdge, normalEdge));
    }
    // Thin distant lines turn to noise, so fade outlines with distance.
    return edge * (1.0 - smoothstep(60.0, 160.0, depthCenter));
}

// ACES filmic curve (Narkowicz 2015 fit).
vec3 ToneMapACES(vec3 x) {
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

void main() {
    float depth = texture(uSceneDepth, vUV).r;
    vec3 viewDir = ViewRay(vUV);
    vec3 color;

    if (depth >= 1.0) {
        color = SkyColor(viewDir);
    } else {
        color = texture(uSceneColor, vUV).rgb;
        float distance = LinearDepth(depth);

        if (uOutlinesOn == 1)
            color = mix(color, vec3(0.03, 0.03, 0.05), EdgeStrength(vUV, distance));

        // Aerial perspective: exponential fog, 1 - e^(-density * distance), toward the
        // horizon colour. Capped so even the farthest buildings keep their shape.
        float fog = min(1.0 - exp(-distance * 0.0008), 0.35);
        color = mix(color, SkyColor(vec3(viewDir.x, 0.0, viewDir.z)), fog);
    }

    color = ToneMapACES(color * 0.9);
    color = pow(color, vec3(1.0 / 2.2));

    float vignette = 1.0 - 0.25 * smoothstep(0.45, 1.0, length(vUV - 0.5) * 1.4);
    fragColor = vec4(color * vignette, 1.0);
}
