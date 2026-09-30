#include "ui/TextRenderer.h"
#include <stb_truetype.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <cstdio>

static const char* textVertSrc = R"(
#version 460 core
layout (location = 0) in vec4 vertex;
out vec2 TexCoords;
uniform mat4 projection;
void main() {
    gl_Position = projection * vec4(vertex.xy, 0.0, 1.0);
    TexCoords = vertex.zw;
}
)";

static const char* textFragSrc = R"(
#version 460 core
in vec2 TexCoords;
out vec4 FragColor;
uniform sampler2D textTex;
uniform vec3 textColor;
void main() {
    float a = texture(textTex, TexCoords).r;
    if (a < 0.05) discard;
    FragColor = vec4(textColor, a);
}
)";

static const char* rectVertSrc = R"(
#version 460 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec4 aColor;
out vec4 vColor;
uniform mat4 projection;
void main() {
    gl_Position = projection * vec4(aPos, 0.0, 1.0);
    vColor = aColor;
}
)";

static const char* rectFragSrc = R"(
#version 460 core
in vec4 vColor;
out vec4 FragColor;
void main() { FragColor = vColor; }
)";

bool TextRenderer::Init(const std::string& fontPath, float pixelHeight) {
    std::ifstream file(fontPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;
    auto size = file.tellg();
    file.seekg(0);
    m_fontData.resize(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(m_fontData.data()), size);

    m_fontInfo = new stbtt_fontinfo();
    auto* fi = static_cast<stbtt_fontinfo*>(m_fontInfo);
    if (!stbtt_InitFont(fi, m_fontData.data(), stbtt_GetFontOffsetForIndex(m_fontData.data(), 0)))
        return false;

    m_scale = stbtt_ScaleForPixelHeight(fi, pixelHeight);
    int descent, lineGap;
    stbtt_GetFontVMetrics(fi, &m_ascent, &descent, &lineGap);

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &textVertSrc, nullptr);
    glCompileShader(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &textFragSrc, nullptr);
    glCompileShader(fs);

    m_program = glCreateProgram();
    glAttachShader(m_program, vs);
    glAttachShader(m_program, fs);
    glLinkProgram(m_program);
    glDeleteShader(vs);
    glDeleteShader(fs);

    m_projLoc = glGetUniformLocation(m_program, "projection");
    m_colorLoc = glGetUniformLocation(m_program, "textColor");
    m_texLoc = glGetUniformLocation(m_program, "textTex");

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    GLuint rvs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(rvs, 1, &rectVertSrc, nullptr);
    glCompileShader(rvs);
    GLuint rfs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(rfs, 1, &rectFragSrc, nullptr);
    glCompileShader(rfs);
    m_rectProgram = glCreateProgram();
    glAttachShader(m_rectProgram, rvs);
    glAttachShader(m_rectProgram, rfs);
    glLinkProgram(m_rectProgram);
    glDeleteShader(rvs);
    glDeleteShader(rfs);
    m_rectProjLoc = glGetUniformLocation(m_rectProgram, "projection");

    glGenVertexArrays(1, &m_rectVao);
    glGenBuffers(1, &m_rectVbo);
    glBindVertexArray(m_rectVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_rectVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 6, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return true;
}

void TextRenderer::Shutdown() {
    if (m_fontInfo) { delete static_cast<stbtt_fontinfo*>(m_fontInfo); m_fontInfo = nullptr; }
    for (auto& [k, g] : m_glyphCache) if (g.texture) glDeleteTextures(1, &g.texture);
    m_glyphCache.clear();
    if (m_program) { glDeleteProgram(m_program); m_program = 0; }
    if (m_vao) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
    if (m_vbo) { glDeleteBuffers(1, &m_vbo); m_vbo = 0; }
    if (m_rectProgram) { glDeleteProgram(m_rectProgram); m_rectProgram = 0; }
    if (m_rectVao) { glDeleteVertexArrays(1, &m_rectVao); m_rectVao = 0; }
    if (m_rectVbo) { glDeleteBuffers(1, &m_rectVbo); m_rectVbo = 0; }
}

TextRenderer::GlyphInfo& TextRenderer::GetGlyph(int codepoint) {
    auto it = m_glyphCache.find(codepoint);
    if (it != m_glyphCache.end()) return it->second;

    auto* fi = static_cast<stbtt_fontinfo*>(m_fontInfo);
    GlyphInfo g;

    int advance, lsb;
    stbtt_GetCodepointHMetrics(fi, codepoint, &advance, &lsb);
    g.advance = static_cast<int>(advance * m_scale);
    g.bearingX = static_cast<int>(lsb * m_scale);

    int x0, y0, x1, y1;
    stbtt_GetCodepointBitmapBox(fi, codepoint, m_scale, m_scale, &x0, &y0, &x1, &y1);
    g.width = x1 - x0;
    g.height = y1 - y0;
    g.bearingY = -y0;

    if (g.width > 0 && g.height > 0) {
        std::vector<unsigned char> bitmap(g.width * g.height);
        stbtt_MakeCodepointBitmap(fi, bitmap.data(), g.width, g.height, g.width, m_scale, m_scale, codepoint);

        glGenTextures(1, &g.texture);
        glBindTexture(GL_TEXTURE_2D, g.texture);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, g.width, g.height, 0, GL_RED, GL_UNSIGNED_BYTE, bitmap.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    m_glyphCache[codepoint] = g;
    return m_glyphCache[codepoint];
}

void TextRenderer::BeginFrame(int screenW, int screenH) {
    m_screenW = screenW; m_screenH = screenH;
    glm::mat4 proj = glm::ortho(0.0f, (float)screenW, 0.0f, (float)screenH);
    glUseProgram(m_program);
    glUniformMatrix4fv(m_projLoc, 1, GL_FALSE, glm::value_ptr(proj));
    glUniform1i(m_texLoc, 0);
    glUseProgram(m_rectProgram);
    glUniformMatrix4fv(m_rectProjLoc, 1, GL_FALSE, glm::value_ptr(proj));
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glUseProgram(m_program);
}

static void PushRectQuad(float* out, float x0, float y0, float x1, float y1,
                          float r0, float g0, float b0, float a0,
                          float r1, float g1, float b1, float a1) {
    int i = 0;
    auto put = [&](float x, float y, float r, float g, float b, float a) {
        out[i++]=x; out[i++]=y; out[i++]=r; out[i++]=g; out[i++]=b; out[i++]=a;
    };
    put(x0,y0,r0,g0,b0,a0);
    put(x1,y0,r0,g0,b0,a0);
    put(x1,y1,r1,g1,b1,a1);
    put(x0,y0,r0,g0,b0,a0);
    put(x1,y1,r1,g1,b1,a1);
    put(x0,y1,r1,g1,b1,a1);
}

void TextRenderer::DrawRect(float x, float y, float w, float h, glm::vec3 c, float a) {
    glUseProgram(m_rectProgram);
    float buf[36];
    PushRectQuad(buf, x, y, x+w, y+h, c.r, c.g, c.b, a, c.r, c.g, c.b, a);
    glBindVertexArray(m_rectVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_rectVbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(buf), buf);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glUseProgram(m_program);
}

void TextRenderer::DrawGradientRect(float x, float y, float w, float h,
                                     glm::vec3 top, glm::vec3 bot, float ta, float ba) {
    glUseProgram(m_rectProgram);
    float buf[36];
    PushRectQuad(buf, x, y, x+w, y+h, bot.r, bot.g, bot.b, ba, top.r, top.g, top.b, ta);
    glBindVertexArray(m_rectVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_rectVbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(buf), buf);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glUseProgram(m_program);
}

// A line segment is drawn as a thin quad: offset both end points sideways
// by half the thickness along the segment's normal.
void TextRenderer::DrawLine(float x0, float y0, float x1, float y1, float thickness, glm::vec3 c, float a) {
    glm::vec2 dir(x1 - x0, y1 - y0);
    float len = glm::length(dir);
    if (len < 1e-4f) return;
    glm::vec2 n = glm::vec2(-dir.y, dir.x) / len * (thickness * 0.5f);
    float buf[36];
    int i = 0;
    auto put = [&](glm::vec2 p) {
        buf[i++]=p.x; buf[i++]=p.y; buf[i++]=c.r; buf[i++]=c.g; buf[i++]=c.b; buf[i++]=a;
    };
    glm::vec2 p0(x0, y0), p1(x1, y1);
    put(p0 - n); put(p1 - n); put(p1 + n);
    put(p0 - n); put(p1 + n); put(p0 + n);
    glUseProgram(m_rectProgram);
    glBindVertexArray(m_rectVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_rectVbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(buf), buf);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glUseProgram(m_program);
}

void TextRenderer::DrawRectOutline(float x, float y, float w, float h, float t, glm::vec3 c, float a) {
    DrawRect(x, y+h-t, w, t, c, a);
    DrawRect(x, y, w, t, c, a);
    DrawRect(x, y, t, h, c, a);
    DrawRect(x+w-t, y, t, h, c, a);
}

void TextRenderer::DrawParallelogram(float x, float y, float w, float h, float skew, glm::vec3 c, float a) {
    glUseProgram(m_rectProgram);
    float buf[36];
    int i = 0;
    auto put = [&](float px, float py) {
        buf[i++]=px; buf[i++]=py; buf[i++]=c.r; buf[i++]=c.g; buf[i++]=c.b; buf[i++]=a;
    };
    put(x, y); put(x+w, y); put(x+w+skew, y+h);
    put(x, y); put(x+w+skew, y+h); put(x+skew, y+h);
    glBindVertexArray(m_rectVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_rectVbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(buf), buf);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glUseProgram(m_program);
}

void TextRenderer::DrawTextOutlined(const std::string& text, float x, float y, float scale,
                                     glm::vec3 fill, glm::vec3 outline, float ow) {
    DrawText(text, x - ow, y, scale, outline);
    DrawText(text, x + ow, y, scale, outline);
    DrawText(text, x, y - ow, scale, outline);
    DrawText(text, x, y + ow, scale, outline);
    DrawText(text, x - ow, y - ow, scale, outline);
    DrawText(text, x + ow, y + ow, scale, outline);
    DrawText(text, x, y, scale, fill);
}

void TextRenderer::DrawText(const std::string& text, float x, float y, float scale, glm::vec3 color) {
    if (!m_program || !m_fontInfo) return;

    glUseProgram(m_program);   // several TextRenderers (fonts) may be interleaved
    glUniform3f(m_colorLoc, color.r, color.g, color.b);

    const unsigned char* s = reinterpret_cast<const unsigned char*>(text.c_str());
    float curX = x;

    while (*s) {
        uint32_t cp = 0;
        if ((*s & 0xF0) == 0xE0) { cp = (s[0]&0x0F)<<12|(s[1]&0x3F)<<6|(s[2]&0x3F); s+=3; }
        else if ((*s & 0xE0) == 0xC0) { cp = (s[0]&0x1F)<<6|(s[1]&0x3F); s+=2; }
        else { cp = *s++; }

        auto& g = GetGlyph(cp);
        if (g.texture) {
            float xp = curX + g.bearingX * scale;
            float yp = y - (g.height - g.bearingY) * scale;
            float w = g.width * scale;
            float h = g.height * scale;

            float verts[6][4] = {
                {xp, yp+h, 0, 0}, {xp, yp, 0, 1}, {xp+w, yp, 1, 1},
                {xp, yp+h, 0, 0}, {xp+w, yp, 1, 1}, {xp+w, yp+h, 1, 0},
            };

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, g.texture);
            glBindVertexArray(m_vao);
            glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }
        curX += g.advance * scale;
    }
}

void TextRenderer::EndFrame() {
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}

float TextRenderer::MeasureText(const std::string& text, float scale) {
    const unsigned char* s = reinterpret_cast<const unsigned char*>(text.c_str());
    float w = 0;
    while (*s) {
        uint32_t cp = 0;
        if ((*s & 0xF0) == 0xE0) { cp = (s[0]&0x0F)<<12|(s[1]&0x3F)<<6|(s[2]&0x3F); s+=3; }
        else if ((*s & 0xE0) == 0xC0) { cp = (s[0]&0x1F)<<6|(s[1]&0x3F); s+=2; }
        else { cp = *s++; }
        auto& g = GetGlyph(cp);
        w += g.advance * scale;
    }
    return w;
}
