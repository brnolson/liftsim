#version 460 core
// Shadow pass: only depth as seen from the sun is needed. Instanced geometry
// sways with the same wind as scene.vert so shadows follow the foliage.
layout (location = 0) in vec3 aPos;
layout (location = 3) in mat4 aInstanceModel;
layout (location = 7) in vec4 aInstanceColor;

uniform int   uInstanced;
uniform mat4  uModel;
uniform mat4  uLightViewProj;
uniform float uTime;

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
    } else {
        world = uModel * vec4(aPos, 1.0);
    }
    gl_Position = uLightViewProj * world;
}
