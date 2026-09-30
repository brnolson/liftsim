#include "render/OrbitCamera.h"
#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

void OrbitCamera::Orbit(float dYawDeg, float dPitchDeg) {
    m_yaw += dYawDeg;
    m_pitch = std::clamp(m_pitch + dPitchDeg, -10.0f, 85.0f);
}

void OrbitCamera::Zoom(float wheelSteps) {
    // Multiplicative zoom feels uniform whether close up or far away.
    m_distance = std::clamp(m_distance * std::pow(0.88f, wheelSteps), 4.0f, 200.0f);
}

void OrbitCamera::Pan(float dx, float dy) {
    glm::vec3 forward = glm::normalize(m_target - Position());
    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
    float scale = m_distance * 0.0015f;
    m_desiredTarget += (-right * dx + glm::vec3(0, 1, 0) * dy) * scale;
    m_target = m_desiredTarget;
}

void OrbitCamera::Set(const glm::vec3& target, float yawDeg, float pitchDeg, float distance) {
    m_target = m_desiredTarget = target;
    m_yaw = yawDeg;
    m_pitch = pitchDeg;
    m_distance = distance;
}

void OrbitCamera::Update(float dt) {
    // Exponential smoothing: frame-rate independent ease toward the goal.
    float blend = 1.0f - std::exp(-4.0f * dt);
    m_target += (m_desiredTarget - m_target) * blend;
}

glm::vec3 OrbitCamera::Position() const {
    float yaw = glm::radians(m_yaw), pitch = glm::radians(m_pitch);
    glm::vec3 offset(std::sin(yaw) * std::cos(pitch),
                     std::sin(pitch),
                     std::cos(yaw) * std::cos(pitch));
    return m_target + offset * m_distance;
}

glm::mat4 OrbitCamera::View() const {
    return glm::lookAt(Position(), m_target, glm::vec3(0, 1, 0));
}

glm::mat4 OrbitCamera::Projection(float aspect) const {
    return glm::perspective(glm::radians(m_fovDeg), aspect, m_near, m_far);
}

Ray OrbitCamera::ScreenRay(float mouseX, float mouseY, int width, int height) const {
    // Pixel -> normalized device coordinates (-1..1, y up) -> world space by
    // un-projecting points on the near and far planes.
    float ndcX = 2.0f * mouseX / width - 1.0f;
    float ndcY = 1.0f - 2.0f * mouseY / height;
    glm::mat4 inverse = glm::inverse(Projection(float(width) / height) * View());
    glm::vec4 nearPoint = inverse * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
    glm::vec4 farPoint = inverse * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
    glm::vec3 a = glm::vec3(nearPoint) / nearPoint.w;
    glm::vec3 b = glm::vec3(farPoint) / farPoint.w;
    return {a, glm::normalize(b - a)};
}
