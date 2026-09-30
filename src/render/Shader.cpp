#include "render/Shader.h"
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <sstream>
#include <cstdio>

Shader::~Shader() {
    if (m_program)
        glDeleteProgram(m_program);
}

Shader::Shader(Shader&& other) noexcept
    : m_program(other.m_program),
      m_uniformCache(std::move(other.m_uniformCache)),
      m_vec3Cache(std::move(other.m_vec3Cache)) {
    other.m_program = 0;
}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        if (m_program)
            glDeleteProgram(m_program);
        m_program = other.m_program;
        m_uniformCache = std::move(other.m_uniformCache);
        m_vec3Cache = std::move(other.m_vec3Cache);
        other.m_program = 0;
    }
    return *this;
}

bool Shader::Load(const std::string& vertexPath, const std::string& fragmentPath) {
    std::string vertSrc = ReadFile(vertexPath);
    std::string fragSrc = ReadFile(fragmentPath);
    if (vertSrc.empty() || fragSrc.empty())
        return false;

    GLuint vert = CompileShader(GL_VERTEX_SHADER, vertSrc, vertexPath);
    GLuint frag = CompileShader(GL_FRAGMENT_SHADER, fragSrc, fragmentPath);
    if (!vert || !frag) {
        if (vert) glDeleteShader(vert);
        if (frag) glDeleteShader(frag);
        return false;
    }

    m_program = glCreateProgram();
    glAttachShader(m_program, vert);
    glAttachShader(m_program, frag);
    glLinkProgram(m_program);

    GLint success;
    glGetProgramiv(m_program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(m_program, 512, nullptr, log);
        std::fprintf(stderr, "Shader link error:\n%s\n", log);
        glDeleteProgram(m_program);
        m_program = 0;
    }

    glDeleteShader(vert);
    glDeleteShader(frag);
    m_uniformCache.clear();
    m_vec3Cache.clear();

    return m_program != 0;
}

void Shader::Use() const {
    glUseProgram(m_program);
}

void Shader::SetInt(const std::string& name, int value) {
    glUniform1i(GetUniformLocation(name), value);
}

void Shader::SetFloat(const std::string& name, float value) {
    glUniform1f(GetUniformLocation(name), value);
}

void Shader::SetVec2(const std::string& name, const glm::vec2& value) {
    glUniform2fv(GetUniformLocation(name), 1, glm::value_ptr(value));
}

void Shader::SetVec3(const std::string& name, const glm::vec3& value) {
    GLint loc = GetUniformLocation(name);
    if (loc < 0) return;
    auto it = m_vec3Cache.find(loc);
    if (it != m_vec3Cache.end() && it->second == value) return;
    m_vec3Cache[loc] = value;
    glUniform3fv(loc, 1, glm::value_ptr(value));
}

void Shader::SetVec4(const std::string& name, const glm::vec4& value) {
    glUniform4fv(GetUniformLocation(name), 1, glm::value_ptr(value));
}

void Shader::SetMat3(const std::string& name, const glm::mat3& value) {
    glUniformMatrix3fv(GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
}

void Shader::SetMat4(const std::string& name, const glm::mat4& value) {
    glUniformMatrix4fv(GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
}

void Shader::SetVec3Array(const std::string& name, const glm::vec3* values, int count) {
    if (count > 0) glUniform3fv(GetUniformLocation(name), count, glm::value_ptr(values[0]));
}

GLint Shader::GetUniformLocation(const std::string& name) {
    auto it = m_uniformCache.find(name);
    if (it != m_uniformCache.end())
        return it->second;

    GLint loc = glGetUniformLocation(m_program, name.c_str());
    m_uniformCache[name] = loc;
    return loc;
}

std::string Shader::ReadFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::fprintf(stderr, "Failed to open shader: %s\n", path.c_str());
        return "";
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

GLuint Shader::CompileShader(GLenum type, const std::string& source, const std::string& path) {
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, 512, nullptr, log);
        const char* typeStr = (type == GL_VERTEX_SHADER) ? "VERTEX" : "FRAGMENT";
        std::fprintf(stderr, "%s shader error in %s:\n%s\n", typeStr, path.c_str(), log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}
