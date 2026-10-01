#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 3) in mat4 aInstanceModel;   // per instance, occupies locations 3-6
layout (location = 7) in vec3 aInstanceColor;   // sRGB albedo

uniform int   uInstanced;      // 1 when drawing an InstanceBatch
uniform mat4  uModel;          // non-instanced draws
uniform mat3  uNormalMatrix;   // inverse-transpose of uModel, computed on the CPU
uniform vec3  uColor;
uniform mat4  uView;
uniform mat4  uProjection;

out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vAlbedo;

void main() {
    vec4 world;
    if (uInstanced == 1) {
        world = aInstanceModel * vec4(aPos, 1.0);
        vNormal = transpose(inverse(mat3(aInstanceModel))) * aNormal;
        vAlbedo = aInstanceColor;
    } else {
        world = uModel * vec4(aPos, 1.0);
        vNormal = uNormalMatrix * aNormal;
        vAlbedo = uColor;
    }
    vWorldPos = world.xyz;
    gl_Position = uProjection * uView * world;
}
