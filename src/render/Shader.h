#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>

class Shader {
public:
    Shader() = default;
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    bool Load(const std::string& vertexPath, const std::string& fragmentPath);
    void Use() const;

    void SetInt(const std::string& name, int value);
    void SetFloat(const std::string& name, float value);
    void SetVec2(const std::string& name, const glm::vec2& value);
    void SetVec3(const std::string& name, const glm::vec3& value);
    void SetVec4(const std::string& name, const glm::vec4& value);
    void SetMat3(const std::string& name, const glm::mat3& value);
    void SetMat4(const std::string& name, const glm::mat4& value);
    void SetVec3Array(const std::string& name, const glm::vec3* values, int count);

    GLuint GetID() const { return m_program; }
    bool IsValid() const { return m_program != 0; }

private:
    GLint GetUniformLocation(const std::string& name);
    static std::string ReadFile(const std::string& path);
    static GLuint CompileShader(GLenum type, const std::string& source, const std::string& path);

    GLuint m_program = 0;
    std::unordered_map<std::string, GLint> m_uniformCache;
    std::unordered_map<GLint, glm::vec3> m_vec3Cache;
};
