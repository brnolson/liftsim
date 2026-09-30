#include "render/Mesh.h"
#include <cmath>

static constexpr float PI = 3.14159265358979323846f;
static constexpr float TWO_PI = 6.28318530717958647692f;

Mesh::~Mesh() {
    Cleanup();
}

Mesh::Mesh(Mesh&& other) noexcept
    : m_vao(other.m_vao), m_vbo(other.m_vbo), m_ebo(other.m_ebo), m_indexCount(other.m_indexCount) {
    other.m_vao = 0;
    other.m_vbo = 0;
    other.m_ebo = 0;
    other.m_indexCount = 0;
}

Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this != &other) {
        Cleanup();
        m_vao = other.m_vao;
        m_vbo = other.m_vbo;
        m_ebo = other.m_ebo;
        m_indexCount = other.m_indexCount;
        other.m_vao = 0;
        other.m_vbo = 0;
        other.m_ebo = 0;
        other.m_indexCount = 0;
    }
    return *this;
}

void Mesh::Init(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices) {
    Cleanup();
    m_indexCount = static_cast<unsigned int>(indices.size());

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ebo);

    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 vertices.size() * sizeof(Vertex),
                 vertices.data(),
                 GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 indices.size() * sizeof(unsigned int),
                 indices.data(),
                 GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, position)));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, normal)));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, texCoords)));

    glBindVertexArray(0);
}

void Mesh::Draw() const {
    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr);
}

void Mesh::Cleanup() {
    if (m_vao) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
    if (m_vbo) { glDeleteBuffers(1, &m_vbo); m_vbo = 0; }
    if (m_ebo) { glDeleteBuffers(1, &m_ebo); m_ebo = 0; }
    m_indexCount = 0;
}

Mesh Mesh::CreateCube(float size) {
    float h = size * 0.5f;

    std::vector<Vertex> vertices = {
        // Front face (+Z)
        {{-h, -h,  h}, { 0,  0,  1}, {0, 0}},
        {{ h, -h,  h}, { 0,  0,  1}, {1, 0}},
        {{ h,  h,  h}, { 0,  0,  1}, {1, 1}},
        {{-h,  h,  h}, { 0,  0,  1}, {0, 1}},
        // Back face (-Z)
        {{ h, -h, -h}, { 0,  0, -1}, {0, 0}},
        {{-h, -h, -h}, { 0,  0, -1}, {1, 0}},
        {{-h,  h, -h}, { 0,  0, -1}, {1, 1}},
        {{ h,  h, -h}, { 0,  0, -1}, {0, 1}},
        // Top face (+Y)
        {{-h,  h,  h}, { 0,  1,  0}, {0, 0}},
        {{ h,  h,  h}, { 0,  1,  0}, {1, 0}},
        {{ h,  h, -h}, { 0,  1,  0}, {1, 1}},
        {{-h,  h, -h}, { 0,  1,  0}, {0, 1}},
        // Bottom face (-Y)
        {{-h, -h, -h}, { 0, -1,  0}, {0, 0}},
        {{ h, -h, -h}, { 0, -1,  0}, {1, 0}},
        {{ h, -h,  h}, { 0, -1,  0}, {1, 1}},
        {{-h, -h,  h}, { 0, -1,  0}, {0, 1}},
        // Right face (+X)
        {{ h, -h,  h}, { 1,  0,  0}, {0, 0}},
        {{ h, -h, -h}, { 1,  0,  0}, {1, 0}},
        {{ h,  h, -h}, { 1,  0,  0}, {1, 1}},
        {{ h,  h,  h}, { 1,  0,  0}, {0, 1}},
        // Left face (-X)
        {{-h, -h, -h}, {-1,  0,  0}, {0, 0}},
        {{-h, -h,  h}, {-1,  0,  0}, {1, 0}},
        {{-h,  h,  h}, {-1,  0,  0}, {1, 1}},
        {{-h,  h, -h}, {-1,  0,  0}, {0, 1}},
    };

    std::vector<unsigned int> indices;
    for (unsigned int face = 0; face < 6; face++) {
        unsigned int base = face * 4;
        indices.push_back(base + 0);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
        indices.push_back(base + 0);
    }

    Mesh mesh;
    mesh.Init(vertices, indices);
    return mesh;
}

Mesh Mesh::CreatePlane(float size) {
    float h = size * 0.5f;

    std::vector<Vertex> vertices = {
        {{-h, 0, -h}, {0, 1, 0}, {0, 0}},
        {{ h, 0, -h}, {0, 1, 0}, {1, 0}},
        {{ h, 0,  h}, {0, 1, 0}, {1, 1}},
        {{-h, 0,  h}, {0, 1, 0}, {0, 1}},
    };

    std::vector<unsigned int> indices = {0, 1, 2, 2, 3, 0};

    Mesh mesh;
    mesh.Init(vertices, indices);
    return mesh;
}

Mesh Mesh::CreateSphere(float radius, int segs, int rings) {
    std::vector<Vertex> v;
    std::vector<unsigned int> idx;
    AppendSphere(v, idx, glm::vec3(0.0f), radius, segs, rings);
    Mesh m;
    m.Init(v, idx);
    return m;
}

Mesh Mesh::CreateCylinder(float radius, float height, int segs) {
    std::vector<Vertex> v;
    std::vector<unsigned int> idx;
    AppendCylinder(v, idx, glm::vec3(0.0f), radius, height * 0.5f, segs);
    Mesh m;
    m.Init(v, idx);
    return m;
}

void Mesh::AppendSphere(std::vector<Vertex>& v, std::vector<unsigned int>& idx,
                        glm::vec3 center, float radius, int segs, int rings) {
    unsigned int base = static_cast<unsigned int>(v.size());
    for (int r = 0; r <= rings; r++) {
        float phi = static_cast<float>(r) / rings * PI;
        float y = std::cos(phi);
        float ringR = std::sin(phi);
        for (int s = 0; s <= segs; s++) {
            float theta = static_cast<float>(s) / segs * TWO_PI;
            float x = std::cos(theta) * ringR;
            float z = std::sin(theta) * ringR;
            glm::vec3 n(x, y, z);
            v.push_back({center + n * radius, n,
                         {static_cast<float>(s)/segs, static_cast<float>(r)/rings}});
        }
    }
    int stride = segs + 1;
    for (int r = 0; r < rings; r++) {
        for (int s = 0; s < segs; s++) {
            unsigned int a = base + r * stride + s;
            unsigned int b = a + 1;
            unsigned int c = a + stride;
            unsigned int d = c + 1;
            idx.insert(idx.end(), {a, b, c, c, b, d});
        }
    }
}

void Mesh::AppendCylinder(std::vector<Vertex>& v, std::vector<unsigned int>& idx,
                          glm::vec3 center, float radius, float halfHeight, int segs) {
    // Side ring — duplicated top/bottom for flat-normal side.
    unsigned int sideBase = static_cast<unsigned int>(v.size());
    for (int s = 0; s <= segs; s++) {
        float a = static_cast<float>(s) / segs * TWO_PI;
        float cx = std::cos(a), cz = std::sin(a);
        glm::vec3 n(cx, 0.0f, cz);
        v.push_back({center + glm::vec3(cx*radius, -halfHeight, cz*radius), n,
                     {static_cast<float>(s)/segs, 0.0f}});
        v.push_back({center + glm::vec3(cx*radius,  halfHeight, cz*radius), n,
                     {static_cast<float>(s)/segs, 1.0f}});
    }
    for (int s = 0; s < segs; s++) {
        unsigned int a = sideBase + s*2;
        unsigned int b = a + 1;
        unsigned int c = a + 2;
        unsigned int d = a + 3;
        idx.insert(idx.end(), {a, b, c, c, b, d});
    }
    // Top cap
    unsigned int topC = static_cast<unsigned int>(v.size());
    v.push_back({center + glm::vec3(0.0f, halfHeight, 0.0f), {0,1,0}, {0.5f, 0.5f}});
    unsigned int topRing = static_cast<unsigned int>(v.size());
    for (int s = 0; s <= segs; s++) {
        float a = static_cast<float>(s) / segs * TWO_PI;
        float cx = std::cos(a), cz = std::sin(a);
        v.push_back({center + glm::vec3(cx*radius, halfHeight, cz*radius), {0,1,0},
                     {cx*0.5f+0.5f, cz*0.5f+0.5f}});
    }
    for (int s = 0; s < segs; s++) {
        idx.insert(idx.end(), {topC, topRing + s + 1, topRing + s});
    }
    // Bottom cap
    unsigned int botC = static_cast<unsigned int>(v.size());
    v.push_back({center + glm::vec3(0.0f, -halfHeight, 0.0f), {0,-1,0}, {0.5f, 0.5f}});
    unsigned int botRing = static_cast<unsigned int>(v.size());
    for (int s = 0; s <= segs; s++) {
        float a = static_cast<float>(s) / segs * TWO_PI;
        float cx = std::cos(a), cz = std::sin(a);
        v.push_back({center + glm::vec3(cx*radius, -halfHeight, cz*radius), {0,-1,0},
                     {cx*0.5f+0.5f, cz*0.5f+0.5f}});
    }
    for (int s = 0; s < segs; s++) {
        idx.insert(idx.end(), {botC, botRing + s, botRing + s + 1});
    }
}

void Mesh::AppendCylinderBetween(std::vector<Vertex>& v, std::vector<unsigned int>& idx,
                                 glm::vec3 p0, glm::vec3 p1, float radius, int segs) {
    glm::vec3 axis = p1 - p0;
    float len = glm::length(axis);
    if (len < 1e-6f) return;
    glm::vec3 dir = axis / len;
    glm::vec3 up = (std::abs(dir.y) < 0.9f) ? glm::vec3(0,1,0) : glm::vec3(1,0,0);
    glm::vec3 right = glm::normalize(glm::cross(up, dir));
    glm::vec3 fwd = glm::cross(dir, right);

    unsigned int base = static_cast<unsigned int>(v.size());
    for (int s = 0; s <= segs; s++) {
        float a = static_cast<float>(s) / segs * TWO_PI;
        float cx = std::cos(a), cz = std::sin(a);
        glm::vec3 n = right * cx + fwd * cz;
        v.push_back({p0 + n * radius, n, {static_cast<float>(s)/segs, 0.0f}});
        v.push_back({p1 + n * radius, n, {static_cast<float>(s)/segs, 1.0f}});
    }
    for (int s = 0; s < segs; s++) {
        unsigned int a = base + s*2;
        unsigned int b = a + 1;
        unsigned int c = a + 2;
        unsigned int d = a + 3;
        idx.insert(idx.end(), {a, b, c, c, b, d});
    }
}

void Mesh::AppendTorus(std::vector<Vertex>& v, std::vector<unsigned int>& idx,
                       glm::vec3 center, float majorR, float minorR,
                       int majorSegs, int minorSegs) {
    unsigned int base = static_cast<unsigned int>(v.size());
    for (int i = 0; i <= majorSegs; i++) {
        float u = static_cast<float>(i) / majorSegs * TWO_PI;
        float cu = std::cos(u), su = std::sin(u);
        for (int j = 0; j <= minorSegs; j++) {
            float vAng = static_cast<float>(j) / minorSegs * TWO_PI;
            float cv = std::cos(vAng), sv = std::sin(vAng);
            glm::vec3 p(
                (majorR + minorR * cv) * cu,
                minorR * sv,
                (majorR + minorR * cv) * su);
            glm::vec3 n(cv * cu, sv, cv * su);
            v.push_back({center + p, n,
                         {static_cast<float>(i)/majorSegs, static_cast<float>(j)/minorSegs}});
        }
    }
    int stride = minorSegs + 1;
    for (int i = 0; i < majorSegs; i++) {
        for (int j = 0; j < minorSegs; j++) {
            unsigned int a = base + i * stride + j;
            unsigned int b = a + 1;
            unsigned int c = a + stride;
            unsigned int d = c + 1;
            idx.insert(idx.end(), {a, b, c, c, b, d});
        }
    }
}
