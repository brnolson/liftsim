#pragma once

namespace sim {

struct MotionLimits {
    float maxSpeed      = 2.5f;    // m/s
    float maxAccel      = 1.0f;    // m/s^2
    float maxJerk       = 1.5f;    // m/s^3
    float levelingSpeed = 0.03f;   // m/s
    float stopTolerance = 0.003f;  // m
};

// Moves the car along the hoistway the way a real elevator drive does:
//   1. Profile generator: turns "distance to go" into a speed reference that is
//      always slow enough to stop in the remaining distance (an S-curve).
//   2. Speed regulator: turns speed error into an acceleration request,
//      with feed-forward from the profile so it tracks without lag.
//   3. Jerk limiter: acceleration may only change at maxJerk. This is what
//      passengers perceive as a smooth ride.
class MotionController {
public:
    enum class Mode {
        Stopped,        // brake applied
        Profile,        // normal run toward a target position
        Uncontrolled,   // injected drive fault: acceleration no longer regulated
        EmergencyStop   // safety gear or brake: constant retardation to zero
    };

    explicit MotionController(const MotionLimits& limits = {}) : m_limits(limits) {}

    void Reset(float position);
    void MoveTo(float target);           // start a run or retarget an active one
    void ForceAcceleration(float accel); // fault injection
    void EmergencyStop(float decel);
    void Update(float dt);

    float Position() const { return m_pos; }
    float Velocity() const { return m_vel; }
    float Acceleration() const { return m_acc; }
    float Jerk() const { return m_jerk; }
    float Target() const { return m_target; }
    Mode  GetMode() const { return m_mode; }
    bool  IsStopped() const { return m_mode == Mode::Stopped; }
    float LastStopError() const { return m_lastStopError; }
    const MotionLimits& Limits() const { return m_limits; }

    // Distance travelled while decelerating from `speed` to rest with an
    // S-curve: v^2 / (2A) + v*A / (2J).
    static float StoppingDistance(float speed, float accel, float jerk);
    // Inverse of StoppingDistance: the fastest speed that can still stop
    // within `distance`.
    static float SpeedForStoppingDistance(float distance, float accel, float jerk);
    // Estimated door-to-door run time for a trip, used by the dispatcher.
    static float FlightTime(float distance, const MotionLimits& limits);

private:
    void UpdateProfile(float dt);
    void SlewAcceleration(float accelRequest, float dt);
    void Integrate(float dt);
    float PatternSpeed(float distToGo) const;

    MotionLimits m_limits;
    Mode  m_mode = Mode::Stopped;
    float m_pos = 0.0f, m_vel = 0.0f, m_acc = 0.0f, m_jerk = 0.0f;
    float m_target = 0.0f;
    float m_forcedAccel = 0.0f;
    float m_emergencyDecel = 0.0f;
    float m_lastStopError = 0.0f;
};

}  // namespace sim
