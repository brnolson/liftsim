#include "sim/Car.h"
#include <algorithm>
#include <cmath>

namespace sim {

const char* ToString(CarMode mode) {
    switch (mode) {
    case CarMode::Idle:          return "IDLE";
    case CarMode::Running:       return "RUNNING";
    case CarMode::DoorsOpening:  return "OPENING";
    case CarMode::DoorsOpen:     return "DOORS OPEN";
    case CarMode::DoorsClosing:  return "CLOSING";
    case CarMode::OutOfService:  return "OUT OF SERVICE";
    }
    return "?";
}

static MotionLimits LimitsFrom(const ElevatorSpec& s) {
    return {s.ratedSpeed, s.maxAccel, s.maxJerk, s.levelingSpeed, s.stopTolerance};
}

Car::Car(int id, const ElevatorSpec& spec, const BuildingSpec& building)
    : m_id(id), m_spec(spec), m_building(building),
      m_motion(LimitsFrom(spec)),
      m_doors(spec.doorOpenTime, spec.doorCloseTime) {
    carCalls.assign(building.floorCount, false);
    m_motion.Reset(0.0f);
    m_traction.tractionLimit = std::exp(spec.grooveFriction * spec.TractionWrapAngle());
}

void Car::Update(float dt) {
    m_motion.Update(dt);
    m_doors.Update(dt, beamBlocked);
    UpdateSafety();
    UpdateTraction(dt);
}

// ---------------------------------------------------------------------------
// Safety devices
// ---------------------------------------------------------------------------
void Car::UpdateSafety() {
    m_safety.doorsLocked = m_doors.IsClosed();

    // Overspeed governor: a rope-driven flyweight mechanism that trips at 115%
    // of rated speed and mechanically engages the safety gear on the rails.
    if (m_safety.governorOk &&
        std::abs(Velocity()) > m_spec.governorTripRatio * m_spec.ratedSpeed) {
        m_safety.governorOk = false;
        m_motion.EmergencyStop(m_spec.safetyGearDecel);
        mode = CarMode::OutOfService;
    }

    // Unintended car movement (EN 81-20): the car must not leave the landing
    // while the doors are unlocked.
    if (m_safety.ucmOk && !m_doors.IsClosed() &&
        std::abs(Position() - m_doorZoneCenter) > m_spec.ucmDetectDistance) {
        m_safety.ucmOk = false;
        m_motion.EmergencyStop(m_spec.safetyGearDecel);
        mode = CarMode::OutOfService;
    }
}

// ---------------------------------------------------------------------------
// Traction physics (1:1 roping, car and counterweight hang on either side of
// the drive sheave). Positive acceleration = car accelerating upward.
// ---------------------------------------------------------------------------
SuspensionLoads Car::Suspension() const {
    const float y = Position();
    const float travel = m_building.TravelHeight();
    const float rope = m_spec.HoistRopeMassPerM();
    const float chain = m_spec.compensated ? m_spec.compensationMassPerM : 0.0f;

    // Hoist rope hangs from the sheave down to the car (long when the car is
    // low) and down to the counterweight (long when the car is high).
    float ropeCarSide = rope * (m_spec.ropeHeadroom + travel - y);
    float ropeCwtSide = rope * (m_spec.ropeHeadroom + y);
    // Compensation chain hangs from the underside of each into the pit, so it
    // mirrors the rope: long under the car when the car is high.
    float chainCarSide = chain * (m_spec.pitLoopDepth + y);
    float chainCwtSide = chain * (m_spec.pitLoopDepth + travel - y);
    // The traveling cable hangs in a U-loop of fixed length from mid-hoistway;
    // the car carries the leg down to the bottom of the loop, which lengthens
    // at half the car's travel (geometry in SceneBuilder::AddTravelingCable).
    float cableOnCar = m_spec.travelingCableMassPerM * 0.5f * (y + 1.0f);

    SuspensionLoads s;
    s.carSideKg = m_spec.carMassKg + m_loadKg + ropeCarSide + chainCarSide + cableOnCar;
    s.cwtSideKg = m_spec.CounterweightMassKg() + ropeCwtSide + chainCwtSide;
    s.movingKg = s.carSideKg + s.cwtSideKg + m_spec.rotatingMassKg;
    return s;
}

void Car::UpdateTraction(float dt) {
    float a = Acceleration();
    float v = Velocity();
    SuspensionLoads s = Suspension();

    // Rope tension on each side of the sheave. The counterweight accelerates
    // in the opposite direction to the car.
    float tensionCar = s.carSideKg * (kGravity + a);
    float tensionCwt = s.cwtSideKg * (kGravity - a);

    // The machine supplies the difference, plus the force to spin up the
    // rotating parts (expressed as an equivalent mass at the rope).
    float force = tensionCar - tensionCwt + m_spec.rotatingMassKg * a;
    m_traction.imbalanceKg = s.carSideKg - s.cwtSideKg;
    m_traction.tensionCarN = tensionCar;
    m_traction.tensionCwtN = tensionCwt;
    float mechanicalPowerW = force * v;

    float r = m_spec.SheaveRadius();
    m_traction.ropeForceN = force;
    m_traction.torqueNm = force * r;
    m_traction.sheaveAngle += (v / r) * dt;
    m_traction.sheaveRpm = std::abs(v) / (2.0f * kPi * r) * 60.0f;

    // Motoring draws extra power to cover losses; regeneration returns less.
    float electricalW = mechanicalPowerW > 0.0f ? mechanicalPowerW / m_spec.driveEfficiency
                                                : mechanicalPowerW * m_spec.driveEfficiency;
    m_traction.powerKw = electricalW / 1000.0f;
    if (electricalW > 0.0f) m_energyUsedJ += electricalW * dt;
    else                    m_energyRegenJ -= electricalW * dt;

    // Euler-Eytelwein (capstan) equation: the ropes grip the sheave as long as
    // T_high / T_low <= e^(mu * alpha).
    float high = std::max(tensionCar, tensionCwt);
    float low = std::max(1.0f, std::min(tensionCar, tensionCwt));
    m_traction.tensionRatio = high / low;
}

// ---------------------------------------------------------------------------
// Commands
// ---------------------------------------------------------------------------
bool Car::StartRun(int floor) {
    if (!m_safety.Complete() || m_driveFault) return false;
    m_targetFloor = floor;
    m_motion.MoveTo(m_building.FloorY(floor));
    mode = CarMode::Running;
    return true;
}

void Car::Retarget(int floor) {
    m_targetFloor = floor;
    m_motion.MoveTo(m_building.FloorY(floor));
}

void Car::OpenDoors() {
    if (m_doors.IsClosed()) m_doorZoneCenter = Position();
    m_doors.Open();
    mode = CarMode::DoorsOpening;
}

void Car::CloseDoors() {
    m_doors.Close();
    mode = CarMode::DoorsClosing;
}

void Car::InjectDriveFault() {
    if (m_driveFault || mode == CarMode::OutOfService) return;
    m_driveFault = true;

    // With motor torque and brake lost, the system freewheels toward the
    // heavier side: an empty car rises because the counterweight outweighs it.
    SuspensionLoads s = Suspension();
    float accel = (s.cwtSideKg - s.carSideKg) * kGravity / s.movingKg;

    // Near balance the drift is very slow; enforce a minimum so the governor
    // still trips within a few seconds.
    if (std::abs(accel) < 0.4f) accel = (accel >= 0.0f ? 0.4f : -0.4f);
    m_motion.ForceAcceleration(accel);
    mode = CarMode::Running;
}

void Car::ResetFaults() {
    if (IsRunning()) return;   // technician waits for the car to stop
    m_driveFault = false;
    m_safety.governorOk = true;
    m_safety.ucmOk = true;
    mode = CarMode::Idle;
    // Rescue operation: relevel to the nearest landing so riders can get out.
    carCalls[NearestFloor()] = true;
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------
int Car::NearestFloor() const {
    int f = static_cast<int>(std::lround(Position() / m_building.floorHeight));
    return std::clamp(f, 0, m_building.floorCount - 1);
}

int Car::CommittedFloor() const {
    if (!IsRunning()) return NearestFloor();
    float v = Velocity();
    float stopPoint = Position() + (v >= 0.0f ? 1.0f : -1.0f) *
        MotionController::StoppingDistance(v, m_spec.maxAccel * 0.9f, m_spec.maxJerk * 0.9f);
    float floorIndex = stopPoint / m_building.floorHeight;
    // Round in the direction of travel: we cannot stop at a floor behind the stop point.
    int f = static_cast<int>(v >= 0.0f ? std::ceil(floorIndex - 0.01f) : std::floor(floorIndex + 0.01f));
    return std::clamp(f, 0, m_building.floorCount - 1);
}

bool Car::CanStopAt(int floor) const {
    if (!IsRunning()) return true;
    float v = Velocity();
    float distance = (m_building.FloorY(floor) - Position()) * (v >= 0.0f ? 1.0f : -1.0f);
    float needed = MotionController::StoppingDistance(v, m_spec.maxAccel * 0.9f, m_spec.maxJerk * 0.9f);
    return distance >= needed + 0.1f;   // 10 cm margin for the jerk ramp-in
}

bool Car::HasRoomFor(float massKg) const {
    return static_cast<int>(riders.size()) < m_spec.capacityPersons &&
           m_loadKg + massKg <= m_spec.ratedLoadKg;
}

}  // namespace sim
