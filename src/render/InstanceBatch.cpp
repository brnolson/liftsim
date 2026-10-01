#include "render/InstanceBatch.h"
#include "render/Renderer.h"
#include <cstddef>

InstanceBatch::InstanceBatch(const Mesh* mesh, Material material, Surface surface, bool castsShadow)
    : m_mesh(mesh), m_material(material), m_surface(surface), m_castsShadow(castsShadow) {}

InstanceBatch::~InstanceBatch() {
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
}

void InstanceBatch::Add(const glm::mat4& model, const glm::vec3& color) {
    m_instances.push_back({model, color});
}

void InstanceBatch::Upload() {
    if (!m_vbo) glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, m_instances.size() * sizeof(Instance), m_instances.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    m_uploaded = static_cast<int>(m_instances.size());
}

void InstanceBatch::Draw() const {
    if (m_uploaded == 0) return;
    glBindVertexArray(m_mesh->GetVAO());
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    // A mat4 attribute occupies four consecutive locations (3-6), one column
    // each. Divisor 1 advances these attributes once per instance instead of
    // once per vertex.
    const GLsizei stride = sizeof(Instance);
    for (int column = 0; column < 4; ++column) {
        GLuint location = 3 + column;
        glEnableVertexAttribArray(location);
        glVertexAttribPointer(location, 4, GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<void*>(offsetof(Instance, model) + sizeof(glm::vec4) * column));
        glVertexAttribDivisor(location, 1);
    }
    glEnableVertexAttribArray(7);
    glVertexAttribPointer(7, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(Instance, color)));
    glVertexAttribDivisor(7, 1);

    glDrawElementsInstanced(GL_TRIANGLES, m_mesh->GetIndexCount(), GL_UNSIGNED_INT, nullptr, m_uploaded);

    // The same mesh is also drawn without instancing; switch the arrays back off.
    for (GLuint location = 3; location <= 7; ++location) glDisableVertexAttribArray(location);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}
