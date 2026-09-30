#pragma once

// Physical and operational parameters for the building and its elevators.
// Values are typical for a 25-storey office tower with gearless traction
// machines. Where a number comes from a standard or handbook, the source is
// noted here and explained in docs/ELEVATOR_ENGINEERING.md.

namespace sim {

constexpr float kGravity = 9.81f;           // m/s^2
constexpr float kPi = 3.14159265358979f;
constexpr float kPassengerMassKg = 75.0f;   // design mass per person (EN 81-20)

struct ElevatorSpec {
    // --- Ride quality limits (CIBSE Guide D, typical mid/high-rise office) ---
    float ratedSpeed    = 2.5f;    // m/s
    float maxAccel      = 1.0f;    // m/s^2
    float maxJerk       = 1.5f;    // m/s^3
    float levelingSpeed = 0.03f;   // m/s  final creep into the landing
    float stopTolerance = 0.003f;  // m    brake drops inside this window

    // --- Masses ---
    float carMassKg        = 1800.0f;
    float ratedLoadKg      = 1600.0f;  // 21 persons x 75 kg
    int   capacityPersons  = 21;
    float balanceRatio     = 0.45f;    // counterweight = car + 45% of rated load
    float rotatingMassKg   = 400.0f;   // motor rotor + sheaves, referred to the ropes

    // --- Traction machine (gearless PM synchronous, 1:1 roping) ---
    float sheaveDiameter   = 0.64f;    // m
    float driveEfficiency  = 0.85f;    // motor + inverter
    float grooveFriction   = 0.20f;    // effective friction of an undercut groove
    float wrapAngle        = kPi;      // rad (180 degree single wrap)

    // --- Doors: center-opening, 1100 mm clear opening ---
    float doorOpenTime     = 1.8f;     // s
    float doorCloseTime    = 2.4f;     // s  closing is slower (kinetic energy limit)
    float doorDwellTime    = 3.0f;     // s  held open when nobody transfers
    float transferTime     = 1.2f;     // s  per passenger boarding or alighting

    // --- Safety (EN 81-20) ---
    float governorTripRatio = 1.15f;             // governor trips at >= 115% rated speed
    float safetyGearDecel   = 0.5f * kGravity;   // average retardation must be 0.2g..1.0g
    float ucmDetectDistance = 0.15f;             // m moved with doors open before UCM trips
    float fullLoadBypass    = 0.80f;             // skip hall calls above 80% load

    float CounterweightMassKg() const { return carMassKg + balanceRatio * ratedLoadKg; }
    float SheaveRadius() const { return sheaveDiameter * 0.5f; }
};

struct BuildingSpec {
    int   floorCount         = 25;     // floor 0 is the main lobby
    float floorHeight        = 3.6f;   // m, floor-to-floor
    int   carCount           = 4;
    int   populationPerFloor = 40;     // office workers on each upper floor

    float FloorY(int floor) const { return floor * floorHeight; }
    float TravelHeight() const { return (floorCount - 1) * floorHeight; }
    int   Population() const { return (floorCount - 1) * populationPerFloor; }
};

}  // namespace sim
