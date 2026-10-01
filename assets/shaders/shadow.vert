#version 460 core
// Shadow pass: only depth as seen from the sun is needed.
layout (location = 0) in vec3 aPos;
layout (location = 3) in mat4 aInstanceModel;

uniform int   uInstanced;
uniform mat4  uModel;
uniform mat4  uLightViewProj;

void main() {
    vec4 world = (uInstanced == 1) ? aInstanceModel * vec4(aPos, 1.0) : uModel * vec4(aPos, 1.0);
    gl_Position = uLightViewProj * world;
}
