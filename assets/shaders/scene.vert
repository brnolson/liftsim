#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 3) in mat4 aInstanceModel;   // per instance, occupies locations 3-6
layout (location = 7) in vec4 aInstanceColor;   // rgb = albedo, a = wind sway strength

uniform int   uInstanced;      // 1 when drawing an InstanceBatch
uniform mat4  uModel;          // non-instanced draws
uniform mat3  uNormalMatrix;   // inverse-transpose of uModel, computed on the CPU
uniform vec3  uColor;
uniform mat4  uView;
uniform mat4  uProjection;
uniform float uTime;

out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vAlbedo;

// Wind: displace vertices sideways in proportion to the square of their
// height above the instance origin, so trunks and pots stay put while crowns
// sway. Each instance gets its own phase from its position.
// Keep in sync with shadow.vert.
vec3 Wind(vec3 origin, float height, float strength) {
    float phase = origin.x * 0.37 + origin.z * 0.23;
    float bend = strength * 0.015 * height * height;
    return vec3(sin(uTime * 1.3 + phase), 0.0, cos(uTime * 0.9 + phase * 1.7)) * bend;
}

void main() {
    vec4 world;
    if (uInstanced == 1) {
        world = aInstanceModel * vec4(aPos, 1.0);
        vec3 origin = aInstanceModel[3].xyz;
        world.xyz += Wind(origin, max(world.y - origin.y, 0.0), aInstanceColor.a);
        vNormal = transpose(inverse(mat3(aInstanceModel))) * aNormal;
        vAlbedo = aInstanceColor.rgb;
    } else {
        world = uModel * vec4(aPos, 1.0);
        vNormal = uNormalMatrix * aNormal;
        vAlbedo = uColor;
    }
    vWorldPos = world.xyz;
    gl_Position = uProjection * uView * world;
}
