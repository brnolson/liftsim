#version 460 core
// Forward pass: toon lighting, PCF sun shadows, and hybrid ray-traced
// reflections against a simplified box scene.

in vec3 vWorldPos;
in vec3 vNormal;

layout (location = 0) out vec4 outColor;    // linear HDR radiance
layout (location = 1) out vec4 outNormal;   // world normal, encoded to 0..1 (for outlines)

const int MATTE = 0, METAL = 1, POLISHED = 2, EMISSIVE = 3;

uniform vec3 uColor;          // sRGB albedo
uniform int  uMaterial;
uniform vec3 uCameraPos;
uniform vec3 uSunDir;         // direction the light travels
uniform vec3 uSunColor;
uniform int  uShadowsOn;
uniform int  uRayTracingOn;
uniform sampler2DShadow uShadowMap;
uniform mat4 uLightViewProj;

const int MAX_BOXES = 64;
uniform int  uBoxCount;
uniform vec3 uBoxMin[MAX_BOXES];
uniform vec3 uBoxMax[MAX_BOXES];
uniform vec3 uBoxColor[MAX_BOXES];

vec3 ToLinear(vec3 srgb) { return pow(srgb, vec3(2.2)); }

// Keep in sync with SkyColor() in post.frag.
vec3 SkyColor(vec3 dir) {
    float height = clamp(dir.y, 0.0, 1.0);
    vec3 sky = mix(vec3(0.62, 0.72, 0.88), vec3(0.16, 0.34, 0.72), pow(height, 0.5));
    vec3 ground = vec3(0.22, 0.21, 0.20);
    sky = mix(ground, sky, smoothstep(-0.05, 0.02, dir.y));
    float sun = pow(max(dot(dir, -uSunDir), 0.0), 900.0);
    return sky + sun * vec3(20.0, 18.0, 15.0);
}

// Hemisphere ambient: sky light from above, warm bounce light from below.
vec3 Ambient(vec3 n) {
    return mix(vec3(0.30, 0.26, 0.22), vec3(0.42, 0.47, 0.58), n.y * 0.5 + 0.5);
}

// Three flat bands (shadow, half-lit, lit) with narrow soft edges.
float ToonRamp(float x) {
    return 0.35 * smoothstep(0.00, 0.04, x) + 0.65 * smoothstep(0.42, 0.47, x);
}

float SunVisibility(vec3 n) {
    if (uShadowsOn == 0) return 1.0;
    // Normal offset moves the lookup off the surface to suppress self-shadowing.
    vec4 lightClip = uLightViewProj * vec4(vWorldPos + n * 0.05, 1.0);
    vec3 uvz = lightClip.xyz / lightClip.w * 0.5 + 0.5;
    if (uvz.z > 1.0) return 1.0;

    // 3x3 percentage-closer filtering; each tap is already a bilinear compare.
    vec2 texel = 1.0 / vec2(textureSize(uShadowMap, 0));
    float lit = 0.0;
    for (int y = -1; y <= 1; ++y)
        for (int x = -1; x <= 1; ++x)
            lit += texture(uShadowMap, vec3(uvz.xy + vec2(x, y) * texel, uvz.z - 0.0005));
    return lit / 9.0;
}

// ---------------------------------------------------------------------------
// Ray tracing against the box scene
// ---------------------------------------------------------------------------
bool RayBox(vec3 ro, vec3 invDir, vec3 bmin, vec3 bmax, out float tEnter) {
    vec3 t0 = (bmin - ro) * invDir;
    vec3 t1 = (bmax - ro) * invDir;
    vec3 tNear = min(t0, t1);
    vec3 tFar = max(t0, t1);
    tEnter = max(max(tNear.x, tNear.y), tNear.z);
    float tExit = min(min(tFar.x, tFar.y), tFar.z);
    return tEnter > 0.0 && tExit >= tEnter;
}

// The face that was hit is the axis where the point sits closest to the box surface.
vec3 BoxNormal(vec3 p, vec3 bmin, vec3 bmax) {
    vec3 local = (p - 0.5 * (bmin + bmax)) / (0.5 * (bmax - bmin));
    vec3 a = abs(local);
    if (a.x > a.y && a.x > a.z) return vec3(sign(local.x), 0.0, 0.0);
    if (a.y > a.z)              return vec3(0.0, sign(local.y), 0.0);
    return vec3(0.0, 0.0, sign(local.z));
}

vec3 TraceReflection(vec3 ro, vec3 rd) {
    vec3 invDir = 1.0 / rd;
    float nearest = 1e9;
    int hit = -1;
    for (int i = 0; i < uBoxCount; ++i) {
        float t;
        if (RayBox(ro, invDir, uBoxMin[i], uBoxMax[i], t) && t < nearest) {
            nearest = t;
            hit = i;
        }
    }
    if (hit < 0) return SkyColor(rd);

    // One bounce: diffuse shading at the hit point, no secondary shadow ray.
    vec3 p = ro + rd * nearest;
    vec3 n = BoxNormal(p, uBoxMin[hit], uBoxMax[hit]);
    vec3 albedo = ToLinear(uBoxColor[hit]);
    float diffuse = max(dot(n, -uSunDir), 0.0);
    return albedo * (Ambient(n) + uSunColor * diffuse * 0.7);
}

void main() {
    vec3 n = normalize(vNormal);
    vec3 albedo = ToLinear(uColor);
    outNormal = vec4(n * 0.5 + 0.5, 1.0);

    if (uMaterial == EMISSIVE) {
        outColor = vec4(albedo * 4.0, 1.0);    // HDR: stays bright after tone mapping
        return;
    }

    vec3 toLight = -uSunDir;
    vec3 toEye = normalize(uCameraPos - vWorldPos);
    float sun = ToonRamp(max(dot(n, toLight), 0.0) * SunVisibility(n));
    vec3 color = albedo * (Ambient(n) + uSunColor * sun);

    if (uMaterial == METAL || uMaterial == POLISHED) {
        vec3 reflected = reflect(-toEye, n);
        vec3 env = (uRayTracingOn == 1) ? TraceReflection(vWorldPos + n * 0.01, reflected)
                                        : SkyColor(reflected) * 0.4;

        // Schlick Fresnel: reflections strengthen at grazing angles. Metals
        // have a high, albedo-tinted base reflectance; stone is ~4%.
        bool metal = uMaterial == METAL;
        float f0 = metal ? 0.55 : 0.04;
        float fresnel = f0 + (1.0 - f0) * pow(1.0 - max(dot(n, toEye), 0.0), 5.0);
        vec3 tint = metal ? albedo * 1.4 : vec3(1.0);
        color = mix(color, env * tint, fresnel * (metal ? 0.85 : 0.55));

        // Hard-edged toon highlight.
        float highlight = step(0.985, dot(n, normalize(toLight + toEye)));
        color += uSunColor * highlight * sun * 0.8;
    }

    // Soft rim light helps silhouettes read against dark backgrounds.
    float rim = pow(1.0 - max(dot(n, toEye), 0.0), 4.0);
    color += vec3(0.10, 0.12, 0.16) * rim;

    outColor = vec4(color, 1.0);
}
