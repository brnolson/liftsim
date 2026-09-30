#pragma once

#include <glm/glm.hpp>

struct Ray {
    glm::vec3 origin;
    glm::vec3 dir;   // normalized
};

// A camera that orbits a target point on a sphere (yaw/pitch/distance).
// The target can glide smoothly to follow a moving elevator car.
class OrbitCamera {
public:
    void Orbit(float dYawDeg, float dPitchDeg);
    void Zoom(float wheelSteps);
    void Pan(float dx, float dy);          // screen-space drag, scaled by distance
    void SetFollowTarget(const glm::vec3& target) { m_desiredTarget = target; }
    void Set(const glm::vec3& target, float yawDeg, float pitchDeg, float distance);
    void Update(float dt);                  // eases the target toward the desired one

    glm::vec3 Position() const;
    glm::vec3 Target() const { return m_target; }
    glm::mat4 View() const;
    glm::mat4 Projection(float aspect) const;
    float Near() const { return m_near; }
    float Far() const { return m_far; }

    // World-space ray through a pixel, for mouse picking.
    Ray ScreenRay(float mouseX, float mouseY, int width, int height) const;

private:
    glm::vec3 m_target{0.0f, 10.0f, 0.0f};
    glm::vec3 m_desiredTarget{0.0f, 10.0f, 0.0f};
    float m_yaw = -38.0f;        // degrees around the vertical axis
    float m_pitch = 14.0f;       // degrees above the horizon
    float m_distance = 38.0f;
    float m_fovDeg = 45.0f;
    float m_near = 0.2f;
    float m_far = 600.0f;
};
