#include "sim/MotionController.h"
#include <algorithm>
#include <cmath>

namespace sim {

// The speed pattern is planned slightly softer than the hard limits so the
// regulator always has some acceleration and jerk in reserve to correct errors.
static constexpr float kPatternMargin = 0.9f;
// Time constant of the speed loop: how quickly speed error is corrected.
static constexpr float kSpeedLoopTime = 0.25f;

static float Sign(float x) { return x >= 0.0f ? 1.0f : -1.0f; }

void MotionController::Reset(float position) {
    m_pos = m_target = position;
    m_vel = m_acc = m_jerk = 0.0f;
    m_mode = Mode::Stopped;
}

void MotionController::MoveTo(float target) {
    m_target = target;
    if (m_mode == Mode::Stopped) m_mode = Mode::Profile;
}

void MotionController::ForceAcceleration(float accel) {
    m_forcedAccel = accel;
    m_mode = Mode::Uncontrolled;
}

void MotionController::EmergencyStop(float decel) {
    m_emergencyDecel = decel;
    m_mode = Mode::EmergencyStop;
}

void MotionController::Update(float dt) {
    switch (m_mode) {
    case Mode::Stopped:
        m_jerk = 0.0f;
        return;

    case Mode::Profile:
        UpdateProfile(dt);
        return;

    case Mode::FinalStop:
        UpdateFinalStop(dt);
        return;

    case Mode::Uncontrolled:
        SlewAcceleration(m_forcedAccel, dt);
        Integrate(dt);
        return;

    case Mode::EmergencyStop: {
        // A safety gear gripping the rails, or a brake acting on the sheave or
        // ropes: modelled as a constant average retardation, not jerk-limited
        // (an emergency stop is not a comfort stop). Stop exactly when the
        // speed would cross zero.
        float speedLoss = m_emergencyDecel * dt;
        if (std::abs(m_vel) <= speedLoss) {
            m_pos += m_vel * dt * 0.5f;
            m_vel = m_acc = m_jerk = 0.0f;
            m_mode = Mode::Stopped;
        } else {
            m_acc = -Sign(m_vel) * m_emergencyDecel;
            m_jerk = 0.0f;
            Integrate(dt);
        }
        return;
    }
    }
}

// Speed reference for the remaining distance. The floor at levelingSpeed is a
// short creep into the landing: without it the reference would shrink toward
// zero with the distance and the car would crawl the last millimetres. A creep
// approach also makes the final position less sensitive to sensor error.
// (Many modern drives land directly without creep; this is a design choice.)
float MotionController::PatternSpeed(float distToGo) const {
    float a = m_limits.maxAccel * kPatternMargin;
    float j = m_limits.maxJerk * kPatternMargin;
    float v = SpeedForStoppingDistance(distToGo, a, j);
    return std::clamp(v, m_limits.levelingSpeed, m_limits.maxSpeed);
}

void MotionController::UpdateProfile(float dt) {
    float error = m_target - m_pos;       // signed distance to go
    float distToGo = std::abs(error);
    float dir = Sign(error);

    // Work in "along the trip" coordinates: positive = toward the target.
    float speed = m_vel * dir;
    float accel = m_acc * dir;

    // 4) At creep speed, start the final stop ramp just early enough that it
    // ends on the floor. A triangular deceleration from speed v at jerk J lasts
    // T = 2*sqrt(v/J) and covers v*T/2 (half the creep distance, by symmetry).
    if (std::abs(m_vel) <= 1.5f * m_limits.levelingSpeed) {
        float jerk = m_limits.maxJerk * kPatternMargin;
        float rampDistance = std::max(speed, 0.0f) * std::sqrt(std::max(speed, 0.0f) / jerk);
        if (distToGo <= rampDistance + 1e-4f) {
            BeginFinalStop(speed, dir, dt);
            UpdateFinalStop(dt);
            return;
        }
    }

    // Look-ahead: acceleration cannot drop to zero instantly (jerk limit), so
    // while still accelerating the car keeps gaining speed and distance.
    // Predict the speed and remaining distance at the moment accel reaches 0.
    float j = m_limits.maxJerk;
    float speedAhead = speed, distAhead = distToGo;
    if (accel > 0.0f) {
        float rampTime = accel / j;
        speedAhead = speed + accel * accel / (2.0f * j);
        distAhead = distToGo - (speed * rampTime + accel * rampTime * rampTime / 3.0f);
    }
    distAhead = std::max(distAhead, 0.0f);

    // 1) Profile generator: speed reference from the (predicted) distance to go.
    float speedRef = PatternSpeed(distAhead);

    // Feed-forward: how fast the reference itself changes as the car closes in.
    float distNext = std::max(distAhead - std::abs(speed) * dt, 0.0f);
    float accelFF = (PatternSpeed(distNext) - speedRef) / dt;

    // 2) Speed regulator on the predicted speed.
    float accelRequest = accelFF + (speedRef - speedAhead) / kSpeedLoopTime;
    accelRequest = dir * std::clamp(accelRequest, -m_limits.maxAccel, m_limits.maxAccel);

    // 3) Jerk limiter, then integrate.
    SlewAcceleration(accelRequest, dt);
    Integrate(dt);
}

// The drive brings the car to exactly zero speed before the brake drops, so the
// brake only has to hold a stationary sheave. The deceleration is a symmetric
// triangle: jerk -J for the first half of the ramp, +J for the second, so it
// starts and ends at zero with no step in acceleration. In discrete time, n
// steps at jerk j remove j * dt^2 * n^2 / 4 of speed (a sum of a triangle), so
// choose an even n long enough that the required j stays within the limit.
void MotionController::BeginFinalStop(float speed, float dir, float dt) {
    m_mode = Mode::FinalStop;
    m_stopStep = 0;
    if (speed <= 0.0f) {          // already at rest on the floor
        m_stopSteps = 0;
        m_stopJerk = 0.0f;
        return;
    }
    float maxJerk = m_limits.maxJerk * kPatternMargin;
    float rampTime = 2.0f * std::sqrt(speed / maxJerk);
    m_stopSteps = 2 * std::max(1, static_cast<int>(std::ceil(0.5f * rampTime / dt)));
    m_stopJerk = dir * 4.0f * speed / (static_cast<float>(m_stopSteps * m_stopSteps) * dt * dt);
}

void MotionController::UpdateFinalStop(float dt) {
    if (m_stopStep < m_stopSteps) {
        ++m_stopStep;
        m_jerk = (m_stopStep <= m_stopSteps / 2) ? -m_stopJerk : m_stopJerk;
        m_acc += m_jerk * dt;
        Integrate(dt);
    }
    if (m_stopStep >= m_stopSteps) {
        // Speed and acceleration are now zero to within rounding: drop the
        // brake. Any acceleration left over is removed in this step, and the
        // jerk that implies is reported so the ride-quality check sees it.
        m_lastStopError = m_pos - m_target;
        m_jerk = -m_acc / dt;
        m_vel = m_acc = 0.0f;
        m_mode = Mode::Stopped;
    }
}

void MotionController::SlewAcceleration(float accelRequest, float dt) {
    float maxChange = m_limits.maxJerk * dt;
    float change = std::clamp(accelRequest - m_acc, -maxChange, maxChange);
    m_jerk = change / dt;
    m_acc += change;
}

void MotionController::Integrate(float dt) {
    // Semi-implicit Euler: update velocity first, then position with the new velocity.
    m_vel += m_acc * dt;
    m_pos += m_vel * dt;
}

float MotionController::StoppingDistance(float speed, float accel, float jerk) {
    speed = std::abs(speed);
    return speed * speed / (2.0f * accel) + speed * accel / (2.0f * jerk);
}

float MotionController::SpeedForStoppingDistance(float distance, float accel, float jerk) {
    // Solve  v^2/(2A) + vA/(2J) = d  for v (positive root of a quadratic):
    //   v^2 + (A^2/J) v - 2Ad = 0
    float b = accel * accel / jerk;
    return 0.5f * (-b + std::sqrt(b * b + 8.0f * accel * distance));
}

float MotionController::FlightTime(float distance, const MotionLimits& lim) {
    // CIBSE Guide D flight time. If the trip is long enough to reach rated
    // speed:  t = d/v + v/a + a/j.  Otherwise the car peaks at a lower speed
    // halfway through the trip, accelerating then decelerating symmetrically.
    float v = lim.maxSpeed, a = lim.maxAccel, j = lim.maxJerk;
    float fullSpeedDistance = 2.0f * StoppingDistance(v, a, j);
    if (distance >= fullSpeedDistance) return distance / v + v / a + a / j;

    float peak = SpeedForStoppingDistance(distance * 0.5f, a, j);
    return 2.0f * (peak / a + a / j);
}

}  // namespace sim
