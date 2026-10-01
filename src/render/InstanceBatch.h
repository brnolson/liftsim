#pragma once

#include "render/Mesh.h"
#include "render/Surface.h"
#include <glm/glm.hpp>
#include <vector>

enum class Material : int;

// Many copies of one mesh drawn with a single glDrawElementsInstanced call.
// Each instance carries its own model matrix and color in a per-instance vertex buffer (attribute divisor 1), so a whole city of
// towers or a street of trees costs one draw call instead of thousands.
class InstanceBatch {
public:
    struct Instance {
        glm::mat4 model;
        glm::vec3 color;       // sRGB albedo
    };

    InstanceBatch(const Mesh* mesh, Material material, Surface surface, bool castsShadow = true);
    ~InstanceBatch();
    InstanceBatch(const InstanceBatch&) = delete;
    InstanceBatch& operator=(const InstanceBatch&) = delete;

    void Clear() { m_instances.clear(); }
    void Add(const glm::mat4& model, const glm::vec3& color);
    void Upload();          // copy instances to the GPU; call after changes
    void Draw() const;

    Material Mat() const { return m_material; }
    Surface  Surf() const { return m_surface; }
    bool     CastsShadow() const { return m_castsShadow; }
    int      Count() const { return static_cast<int>(m_instances.size()); }

private:
    const Mesh* m_mesh;
    Material m_material;
    Surface m_surface;
    bool m_castsShadow;
    std::vector<Instance> m_instances;
    GLuint m_vbo = 0;
    int m_uploaded = 0;
};
