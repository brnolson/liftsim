#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoords;
};

class Mesh {
public:
    Mesh() = default;
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    void Init(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices);
    void Draw() const;

    unsigned int GetIndexCount() const { return m_indexCount; }
    GLuint GetVAO() const { return m_vao; }

    static Mesh CreateCube(float size = 1.0f);
    static Mesh CreatePlane(float size = 10.0f);
    static Mesh CreateSphere(float radius = 1.0f, int segs = 16, int rings = 10);
    static Mesh CreateCylinder(float radius = 0.5f, float height = 1.0f, int segs = 24);

    // Appenders — add round primitives to existing vertex/index buffers so
    // composite meshes (machine components, etc.) can mix boxes and curves.
    static void AppendSphere(std::vector<Vertex>& v, std::vector<unsigned int>& idx,
                             glm::vec3 center, float radius, int segs = 14, int rings = 9);
    static void AppendCylinder(std::vector<Vertex>& v, std::vector<unsigned int>& idx,
                               glm::vec3 center, float radius, float halfHeight,
                               int segs = 16);
    static void AppendCylinderBetween(std::vector<Vertex>& v, std::vector<unsigned int>& idx,
                                      glm::vec3 p0, glm::vec3 p1, float radius, int segs = 12);
    static void AppendTorus(std::vector<Vertex>& v, std::vector<unsigned int>& idx,
                            glm::vec3 center, float majorR, float minorR,
                            int majorSegs = 20, int minorSegs = 10);

private:
    void Cleanup();

    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLuint m_ebo = 0;
    unsigned int m_indexCount = 0;
};
