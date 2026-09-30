#version 460 core
// Full-screen triangle generated from gl_VertexID (no vertex buffer).
// Vertices (-1,-1), (3,-1), (-1,3) cover the whole viewport with one triangle.
out vec2 vUV;

void main() {
    vec2 pos = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2) * 2.0 - 1.0;
    vUV = pos * 0.5 + 0.5;
    gl_Position = vec4(pos, 0.0, 1.0);
}
