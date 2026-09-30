#pragma once

#include "sim/Direction.h"
#include "sim/DoorOperator.h"
#include "sim/ElevatorSpec.h"
#include "sim/MotionController.h"
#include <algorithm>
#include <vector>

namespace sim {

// The safety chain is a series circuit of safety contacts. If any contact is
// open the drive loses power and the brake drops, so the car cannot run.
struct SafetyChain {
    bool doorsLocked = true;   // car door + landing door interlocks
    bool governorOk  = true;   // overspeed governor electrical switch
    bool ucmOk       = true;   // unintended car movement monitor

    bool Complete() const { return doorsLocked && governorOk && ucmOk; }
};

// Forces, torque and power at the traction sheave for the current instant.
struct TractionState {
    float ropeForceN    = 0.0f;  // net force the machine applies at the sheave rim
    float torqueNm      = 0.0f;
    float powerKw       = 0.0f;  // positive = motoring, negative = regenerating
    float sheaveAngle   = 0.0f;  // rad, integrated for animation
    float sheaveRpm     = 0.0f;
    float tensionRatio  = 1.0f;  // T_high / T_low across the sheave
    float tractionLimit = 1.0f;  // e^(mu*alpha): above this ratio the ropes slip
};

enum class CarMode { Idle, Running, DoorsOpening, DoorsOpen, DoorsClosing, OutOfService };

const char* ToString(CarMode mode);

// One elevator: its motion, doors, drive physics and safety devices.
// Decisions about *where* to go are made by ElevatorSystem (the group controller).
class Car {
public:
    Car(int id, const ElevatorSpec& spec, const BuildingSpec& building);

    void Update(float dt);

    // --- Commands from the group controller ---
    bool StartRun(int floor);          // returns false if the safety chain is open
    void Retarget(int floor);          // stop sooner than planned
    void OpenDoors();
    void CloseDoors();
    void InjectDriveFault();           // speed regulation lost: car runs away
    void ResetFaults();                // technician reset after inspection

    // --- Position ---
    float Position() const { return m_motion.Position(); }   // m, car sill height
    float Velocity() const { return m_motion.Velocity(); }
    float Acceleration() const { return m_motion.Acceleration(); }
    float Jerk() const { return m_motion.Jerk(); }
    int   NearestFloor() const;
    int   TargetFloor() const { return m_targetFloor; }
    bool  IsRunning() const { return !m_motion.IsStopped(); }
    // Nearest floor ahead where the car can still stop comfortably
    // ("advanced car position" in dispatching literature).
    int   CommittedFloor() const;
    bool  CanStopAt(int floor) const;
    float LastStopErrorMm() const { return m_motion.LastStopError() * 1000.0f; }

    // --- Load ---
    float LoadKg() const { return m_loadKg; }
    float LoadFraction() const { return m_loadKg / m_spec.ratedLoadKg; }
    bool  HasRoomFor(float massKg) const;

    const DoorOperator&  Doors() const { return m_doors; }
    const SafetyChain&   Safety() const { return m_safety; }
    const TractionState& Traction() const { return m_traction; }
    const ElevatorSpec&  Spec() const { return m_spec; }
    int   Id() const { return m_id; }
    bool  HasDriveFault() const { return m_driveFault; }
    float EnergyUsedKwh() const { return m_energyUsedJ / 3.6e6f; }
    float EnergyRegenKwh() const { return m_energyRegenJ / 3.6e6f; }

    // --- State owned by the group controller ---
    CarMode           mode = CarMode::Idle;
    Direction         direction = Direction::None;  // travel / hall lantern direction
    std::vector<bool> carCalls;                     // buttons pressed inside the car
    std::vector<int>  riders;                       // passenger ids on board
    float             dwellTimer = 0.0f;
    float             transferTimer = 0.0f;
    bool              beamBlocked = false;          // someone in the doorway

    void AddLoad(float kg) { m_loadKg = std::max(0.0f, m_loadKg + kg); }   // clamp float round-off

private:
    void UpdateSafety();
    void UpdateTraction(float dt);

    int m_id;
    ElevatorSpec m_spec;
    BuildingSpec m_building;
    MotionController m_motion;
    DoorOperator m_doors;
    SafetyChain m_safety;
    TractionState m_traction;

    int   m_targetFloor = 0;
    float m_loadKg = 0.0f;
    float m_doorZoneCenter = 0.0f;    // where the car was when the doors unlocked
    bool  m_driveFault = false;
    float m_energyUsedJ = 0.0f;
    float m_energyRegenJ = 0.0f;
};

}  // namespace sim
