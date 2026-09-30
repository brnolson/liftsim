#include "render/Renderer.h"
#include <algorithm>
#include <cstdio>
#include <glm/gtc/matrix_transform.hpp>

static GLuint MakeTexture(int w, int h, GLenum internalFormat, GLenum format, GLenum type, GLenum filter) {
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, w, h, 0, format, type, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return tex;
}

bool Renderer::Init(int width, int height) {
    if (!m_shadowShader.Load("assets/shaders/shadow.vert", "assets/shaders/shadow.frag")) return false;
    if (!m_sceneShader.Load("assets/shaders/scene.vert", "assets/shaders/scene.frag")) return false;
    if (!m_postShader.Load("assets/shaders/fullscreen.vert", "assets/shaders/post.frag")) return false;

    // Shadow map: a depth texture that can be sampled with hardware depth
    // comparison (sampler2DShadow), which gives filtered 0..1 results.
    m_shadowTex = MakeTexture(kShadowSize, kShadowSize, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glGenFramebuffers(1, &m_shadowFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_shadowTex, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glGenVertexArrays(1, &m_emptyVao);
    CreateTargets(width, height);
    return true;
}

void Renderer::CreateTargets(int width, int height) {
    m_width = width;
    m_height = height;
    m_colorTex = MakeTexture(width, height, GL_RGBA16F, GL_RGBA, GL_FLOAT, GL_LINEAR);
    m_normalTex = MakeTexture(width, height, GL_RGBA16F, GL_RGBA, GL_FLOAT, GL_NEAREST);
    m_depthTex = MakeTexture(width, height, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_FLOAT, GL_NEAREST);

    glGenFramebuffers(1, &m_sceneFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_sceneFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_colorTex, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_normalTex, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depthTex, 0);
    GLenum buffers[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, buffers);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::fprintf(stderr, "Scene framebuffer incomplete\n");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::DestroyTargets() {
    GLuint textures[] = {m_colorTex, m_normalTex, m_depthTex};
    glDeleteTextures(3, textures);
    glDeleteFramebuffers(1, &m_sceneFbo);
    m_colorTex = m_normalTex = m_depthTex = m_sceneFbo = 0;
}

void Renderer::Resize(int width, int height) {
    if (width == m_width && height == m_height) return;
    DestroyTargets();
    CreateTargets(width, height);
}

void Renderer::Shutdown() {
    DestroyTargets();
    glDeleteTextures(1, &m_shadowTex);
    glDeleteFramebuffers(1, &m_shadowFbo);
    glDeleteVertexArrays(1, &m_emptyVao);
}

// Orthographic projection for the sun (a directional light has parallel rays).
// Fit it tightly around the scene bounds so shadow-map texels are not wasted.
glm::mat4 Renderer::LightViewProjection(const AABB& bounds) const {
    glm::vec3 center = (bounds.min + bounds.max) * 0.5f;
    glm::mat4 lightView = glm::lookAt(center - m_sunDir * 100.0f, center, glm::vec3(0, 1, 0));

    glm::vec3 lo(1e9f), hi(-1e9f);
    for (int i = 0; i < 8; ++i) {
        glm::vec3 corner((i & 1) ? bounds.max.x : bounds.min.x,
                         (i & 2) ? bounds.max.y : bounds.min.y,
                         (i & 4) ? bounds.max.z : bounds.min.z);
        glm::vec3 p = glm::vec3(lightView * glm::vec4(corner, 1.0f));
        lo = glm::min(lo, p);
        hi = glm::max(hi, p);
    }
    // View space looks down -z, so near/far are the negated z extents.
    glm::mat4 lightProj = glm::ortho(lo.x, hi.x, lo.y, hi.y, -hi.z - 5.0f, -lo.z + 5.0f);
    return lightProj * lightView;
}

void Renderer::Render(const FrameData& frame) {
    glm::mat4 lightViewProj = LightViewProjection(frame.shadowBounds);
    if (frame.settings.shadows) ShadowPass(frame, lightViewProj);
    ScenePass(frame, lightViewProj);
    PostPass(frame);
}

void Renderer::ShadowPass(const FrameData& frame, const glm::mat4& lightViewProj) {
    glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFbo);
    glViewport(0, 0, kShadowSize, kShadowSize);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);                 // thin panels must cast shadows from both sides
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(2.0f, 4.0f);             // pushes depth back a little: no "shadow acne"

    m_shadowShader.Use();
    m_shadowShader.SetMat4("uLightViewProj", lightViewProj);
    for (const DrawItem& item : *frame.items) {
        if (!item.castsShadow) continue;
        m_shadowShader.SetMat4("uModel", item.model);
        item.mesh->Draw();
    }
    glDisable(GL_POLYGON_OFFSET_FILL);
}

void Renderer::ScenePass(const FrameData& frame, const glm::mat4& lightViewProj) {
    glBindFramebuffer(GL_FRAMEBUFFER, m_sceneFbo);
    glViewport(0, 0, m_width, m_height);
    float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
    float clearNormal[] = {0.5f, 0.5f, 0.5f, 0.0f};
    glClearBufferfv(GL_COLOR, 0, clearColor);
    glClearBufferfv(GL_COLOR, 1, clearNormal);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    Shader& s = m_sceneShader;
    s.Use();
    s.SetMat4("uView", frame.view);
    s.SetMat4("uProjection", frame.projection);
    s.SetMat4("uLightViewProj", lightViewProj);
    s.SetVec3("uCameraPos", frame.cameraPos);
    s.SetVec3("uSunDir", m_sunDir);
    s.SetVec3("uSunColor", m_sunColor);
    s.SetInt("uShadowsOn", frame.settings.shadows ? 1 : 0);
    s.SetInt("uRayTracingOn", frame.settings.rayTracedReflections ? 1 : 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_shadowTex);
    s.SetInt("uShadowMap", 0);

    // Upload the ray-tracing scene: a flat list of boxes (no BVH needed at 64).
    std::vector<glm::vec3> mins, maxs, colors;
    for (const RayBox& rb : *frame.rayBoxes) {
        if (static_cast<int>(mins.size()) == kMaxRayBoxes) break;
        mins.push_back(rb.box.min);
        maxs.push_back(rb.box.max);
        colors.push_back(rb.color);
    }
    s.SetInt("uBoxCount", static_cast<int>(mins.size()));
    s.SetVec3Array("uBoxMin", mins.data(), static_cast<int>(mins.size()));
    s.SetVec3Array("uBoxMax", maxs.data(), static_cast<int>(maxs.size()));
    s.SetVec3Array("uBoxColor", colors.data(), static_cast<int>(colors.size()));

    for (const DrawItem& item : *frame.items) {
        s.SetMat4("uModel", item.model);
        // Normals need the inverse-transpose so non-uniform scaling keeps them perpendicular.
        s.SetMat3("uNormalMatrix", glm::transpose(glm::inverse(glm::mat3(item.model))));
        s.SetVec3("uColor", item.color);
        s.SetInt("uMaterial", static_cast<int>(item.material));
        item.mesh->Draw();
    }
}

void Renderer::PostPass(const FrameData& frame) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, m_width, m_height);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    Shader& s = m_postShader;
    s.Use();
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, m_colorTex);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, m_normalTex);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, m_depthTex);
    s.SetInt("uSceneColor", 0);
    s.SetInt("uSceneNormal", 1);
    s.SetInt("uSceneDepth", 2);
    s.SetVec2("uTexel", glm::vec2(1.0f / m_width, 1.0f / m_height));
    s.SetFloat("uNear", frame.nearPlane);
    s.SetFloat("uFar", frame.farPlane);
    s.SetMat4("uInvViewProj", glm::inverse(frame.projection * frame.view));
    s.SetVec3("uSunDir", m_sunDir);
    s.SetInt("uOutlinesOn", frame.settings.outlines ? 1 : 0);

    glBindVertexArray(m_emptyVao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
}
