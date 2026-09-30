#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <vector>

class TextRenderer {
public:
    bool Init(const std::string& fontPath, float pixelHeight);
    void Shutdown();

    void BeginFrame(int screenW, int screenH);
    void DrawText(const std::string& text, float x, float y, float scale, glm::vec3 color);
    void DrawTextOutlined(const std::string& text, float x, float y, float scale,
                          glm::vec3 fillColor, glm::vec3 outlineColor, float outlineWidth = 3.0f);
    void EndFrame();

    void DrawRect(float x, float y, float w, float h, glm::vec3 color, float alpha = 1.0f);
    void DrawLine(float x0, float y0, float x1, float y1, float thickness, glm::vec3 color, float alpha = 1.0f);
    void DrawRectOutline(float x, float y, float w, float h, float thickness, glm::vec3 color, float alpha = 1.0f);
    void DrawParallelogram(float x, float y, float w, float h, float skew, glm::vec3 color, float alpha = 1.0f);
    void DrawGradientRect(float x, float y, float w, float h,
                          glm::vec3 topColor, glm::vec3 bottomColor, float topAlpha, float bottomAlpha);

    float MeasureText(const std::string& text, float scale);

private:
    struct GlyphInfo {
        GLuint texture = 0;
        int width = 0;
        int height = 0;
        int bearingX = 0;
        int bearingY = 0;
        int advance = 0;
    };

    GlyphInfo& GetGlyph(int codepoint);

    std::vector<unsigned char> m_fontData;
    void* m_fontInfo = nullptr;
    float m_scale = 0.0f;
    int m_ascent = 0;

    std::unordered_map<int, GlyphInfo> m_glyphCache;

    GLuint m_program = 0;
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLint m_projLoc = -1;
    GLint m_colorLoc = -1;
    GLint m_texLoc = -1;

    GLuint m_rectProgram = 0;
    GLuint m_rectVao = 0;
    GLuint m_rectVbo = 0;
    GLint m_rectProjLoc = -1;
    int m_screenW = 0;
    int m_screenH = 0;
};
