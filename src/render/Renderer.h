#pragma once

#include "render/Mesh.h"
#include "render/RayMath.h"
#include "render/Shader.h"
#include <glm/glm.hpp>
#include <vector>

// How a surface responds to light (see scene.frag).
enum class Material : int {
    Matte    = 0,   // toon-shaded diffuse
    Metal    = 1,   // stainless steel: tinted ray-traced reflection
    Polished = 2,   // polished stone floor: faint, Fresnel-weighted reflection
    Emissive = 3    // lamps and lit buttons: unaffected by lighting
};

// One thing to draw: a shared mesh placed in the world by a model matrix.
struct DrawItem {
    const Mesh* mesh = nullptr;
    glm::mat4   model{1.0f};
    glm::vec3   color{1.0f};    // sRGB albedo
    Material    material = Material::Matte;
    bool        castsShadow = true;
};

// A box in the simplified scene that reflection rays are traced against.
struct RayBox {
    AABB      box;
    glm::vec3 color;
};

struct RenderSettings {
    bool shadows = true;
    bool rayTracedReflections = true;
    bool outlines = true;
};

struct FrameData {
    const std::vector<DrawItem>* items = nullptr;
    const std::vector<RayBox>*   rayBoxes = nullptr;
    glm::mat4 view{1.0f}, projection{1.0f};
    glm::vec3 cameraPos{0.0f};
    float     nearPlane = 0.1f, farPlane = 500.0f;
    AABB      shadowBounds{};      // region the shadow map must cover
    RenderSettings settings;
};

// Three-pass pipeline:
//   1. Shadow pass:  scene depth from the sun into a shadow map.
//   2. Scene pass:   toon lighting + PCF shadows + ray-traced reflections into
//                    an HDR color buffer and a normal buffer (G-buffer).
//   3. Post pass:    sky, outlines from depth/normal edges, fog, tone mapping.
class Renderer {
public:
    static constexpr int kMaxRayBoxes = 64;

    bool Init(int width, int height);
    void Resize(int width, int height);
    void Render(const FrameData& frame);
    void Shutdown();

    glm::vec3 SunDirection() const { return m_sunDir; }

private:
    void CreateTargets(int width, int height);
    void DestroyTargets();
    glm::mat4 LightViewProjection(const AABB& bounds) const;
    void ShadowPass(const FrameData& frame, const glm::mat4& lightViewProj);
    void ScenePass(const FrameData& frame, const glm::mat4& lightViewProj);
    void PostPass(const FrameData& frame);

    Shader m_shadowShader, m_sceneShader, m_postShader;
    int m_width = 0, m_height = 0;

    // G-buffer: HDR color, normals, depth
    GLuint m_sceneFbo = 0, m_colorTex = 0, m_normalTex = 0, m_depthTex = 0;
    // Shadow map
    static constexpr int kShadowSize = 4096;
    GLuint m_shadowFbo = 0, m_shadowTex = 0;
    GLuint m_emptyVao = 0;   // full-screen triangle is generated in the vertex shader

    glm::vec3 m_sunDir = glm::normalize(glm::vec3(-0.35f, -0.78f, -0.52f));
    glm::vec3 m_sunColor{1.55f, 1.45f, 1.30f};
};
