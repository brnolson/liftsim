#version 460 core
// Forward pass: procedural surfaces, toon lighting, PCF sun shadows, and
// hybrid ray-traced reflections against a simplified box scene.

in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vAlbedo;              // sRGB albedo (per draw or per instance)

layout (location = 0) out vec4 outColor;    // linear HDR radiance
layout (location = 1) out vec4 outNormal;   // world normal, encoded to 0..1 (for outlines)

const int MATTE = 0, METAL = 1, POLISHED = 2, EMISSIVE = 3;

uniform int  uMaterial;
uniform int  uSurface;        // procedural pattern, see Surface.h
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

// ---------------------------------------------------------------------------
// Procedural surfaces (solid texturing): every pattern is a function of the
// world position, so no UV coordinates or image files are needed.
// ---------------------------------------------------------------------------
const int SURFACE_CONCRETE = 1, SURFACE_TERRAZZO = 2, SURFACE_CARPET = 3, SURFACE_WOOD = 4,
          SURFACE_BRUSHED = 5, SURFACE_ASPHALT = 6, SURFACE_GRASS = 7, SURFACE_FACADE = 8,
          SURFACE_FOLIAGE = 9;

// Pseudo-random value in [0, 1) for each lattice point.
float Hash(vec3 p) {
    p = fract(p * 0.3183099 + vec3(0.71, 0.113, 0.419));
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

// Value noise: random values at integer lattice points, blended with a
// smooth (zero-slope) curve between them.
float Noise(vec3 p) {
    vec3 i = floor(p), f = fract(p);
    vec3 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix(Hash(i + vec3(0, 0, 0)), Hash(i + vec3(1, 0, 0)), u.x),
                   mix(Hash(i + vec3(0, 1, 0)), Hash(i + vec3(1, 1, 0)), u.x), u.y),
               mix(mix(Hash(i + vec3(0, 0, 1)), Hash(i + vec3(1, 0, 1)), u.x),
                   mix(Hash(i + vec3(0, 1, 1)), Hash(i + vec3(1, 1, 1)), u.x), u.y), u.z);
}

// Fractal Brownian motion: octaves of noise, each at double the frequency and
// half the amplitude of the previous one.
float Fbm(vec3 p) {
    float sum = 0.0, amplitude = 0.5;
    for (int i = 0; i < 4; ++i) {
        sum += amplitude * Noise(p);
        p *= 2.03;
        amplitude *= 0.5;
    }
    return sum;
}

// Anti-aliasing for procedural detail. fwidth() gives how far the world
// position moves across one pixel; once a pattern's period is smaller than a
// pixel it can only shimmer, so fade it out (the procedural equivalent of mipmapping).
float Detail(float frequency) {
    float cyclesPerPixel = length(fwidth(vWorldPos)) * frequency;
    return 1.0 - smoothstep(0.25, 0.9, cyclesPerPixel);
}

// Box projection: 2D coordinates on whichever axis-aligned plane the surface faces.
vec2 PlanarUV(vec3 p, vec3 n) {
    vec3 a = abs(n);
    if (a.y >= a.x && a.y >= a.z) return p.xz;
    return a.x > a.z ? p.zy : p.xy;
}

vec3 ApplySurface(vec3 albedo, vec3 p, vec3 n) {
    switch (uSurface) {
    case SURFACE_CONCRETE: {
        float mottling = 0.88 + 0.24 * Fbm(p * 1.7);
        float speckle = step(0.93, Hash(floor(p * 45.0))) * 0.12 * Detail(45.0);
        return albedo * (mottling - speckle);
    }
    case SURFACE_TERRAZZO: {
        // Cellular pattern: each 4.5 cm cell may hold one round stone chip at a
        // random position with a random size and color.
        vec2 uv = p.xz * 22.0;
        vec2 cell = floor(uv), local = fract(uv);
        vec2 center = 0.25 + 0.5 * vec2(Hash(vec3(cell, 1.0)), Hash(vec3(cell, 2.0)));
        float radius = 0.12 + 0.2 * Hash(vec3(cell, 3.0));
        float chip = step(0.55, Hash(vec3(cell, 0.0))) * (1.0 - smoothstep(radius - 0.06, radius, length(local - center)));
        chip *= Detail(22.0) * step(0.5, n.y);   // floor surface only
        vec3 chipColor = mix(vec3(0.20, 0.18, 0.17), vec3(0.55, 0.32, 0.20), Hash(vec3(cell, 7.0)));
        return mix(albedo * (0.95 + 0.1 * Noise(p * 3.0)), chipColor, chip * 0.6);
    }
    case SURFACE_CARPET: {
        vec2 uv = PlanarUV(p, n);
        vec2 tile = floor(uv / 0.6);
        float turned = mod(tile.x + tile.y, 2.0);   // tiles laid in alternating directions
        vec2 pileScale = turned > 0.5 ? vec2(90.0, 25.0) : vec2(25.0, 90.0);
        float pile = Noise(vec3(uv * pileScale, 0.0)) * Detail(90.0);
        float seam = step(0.97, max(fract(uv.x / 0.6), fract(uv.y / 0.6))) * Detail(1.0 / 0.6 * 30.0);
        return albedo * (0.86 + 0.08 * turned + 0.12 * pile - 0.1 * seam);
    }
    case SURFACE_WOOD: {
        // Growth rings around an axis along x, distorted by noise.
        float r = length(p.yz * vec2(1.0, 0.6)) * 18.0 + Fbm(p * vec3(0.8, 3.0, 3.0)) * 4.0;
        float ring = 0.5 + 0.5 * sin(r * 6.2832);
        return albedo * (0.82 + 0.22 * ring);
    }
    case SURFACE_BRUSHED: {
        float streak = Noise(vec3(p.x * 120.0, p.y * 1.5, p.z * 120.0)) * Detail(120.0);   // stretched along y
        return albedo * (0.92 + 0.12 * streak);
    }
    case SURFACE_ASPHALT: {
        float grit = step(0.9, Hash(floor(p * 60.0))) * 0.2 * Detail(60.0);
        return albedo * (0.8 + 0.35 * Fbm(p * 0.6) + grit);
    }
    case SURFACE_GRASS: {
        float patches = Fbm(p * 0.15);
        float blades = Noise(p * vec3(40.0, 1.0, 40.0)) * Detail(40.0);
        return albedo * vec3(0.75 + 0.5 * patches, 0.8 + 0.4 * patches, 0.8) * (0.85 + 0.25 * blades);
    }
    case SURFACE_FACADE: {
        if (abs(n.y) > 0.5) return albedo * (0.85 + 0.2 * Fbm(p * 0.5));   // roof
        vec2 uv = PlanarUV(p, n);
        vec2 bay = vec2(1.8, 3.6);                    // one window per bay per floor
        vec2 cell = floor(uv / bay);
        vec2 local = fract(uv / bay);
        float window = step(0.12, local.x) * step(local.x, 0.88) * step(0.3, local.y) * step(local.y, 0.85);
        float blinds = Hash(vec3(cell, floor(p.x + p.z)));
        vec3 glass = mix(vec3(0.03, 0.05, 0.09), vec3(0.14, 0.20, 0.30), blinds);
        return mix(albedo, glass, window);
    }
    case SURFACE_FOLIAGE: {
        float clusters = Fbm(p * 2.5);
        vec3 tint = mix(vec3(1.0), vec3(1.15, 1.1, 0.7), Hash(floor(p * 3.0)) * 0.4);
        return albedo * (0.6 + 0.8 * clusters) * tint;
    }
    }
    return albedo;
}

// Keep in sync with SkyColor() in post.frag.
vec3 SkyColor(vec3 dir) {
    float height = clamp(dir.y, 0.0, 1.0);
    vec3 haze = vec3(0.62, 0.72, 0.88);
    vec3 sky = mix(haze, vec3(0.16, 0.34, 0.72), pow(height, 0.5));
    sky = mix(haze * 0.85, sky, smoothstep(-0.3, 0.0, dir.y));
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
    vec3 albedo = ApplySurface(ToLinear(vAlbedo), vWorldPos, n);
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
