#pragma once

// Physical and operational parameters for the building and its elevators.
// Values are typical for a 25-storey office tower with gearless traction
// machines. Where a number comes from a standard or handbook, the source is
// noted here and explained in docs/ELEVATOR_ENGINEERING.md.

#include <cmath>

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

    // --- Masses ---
    float carMassKg        = 1800.0f;
    float ratedLoadKg      = 1600.0f;  // 21 persons x 75 kg
    int   capacityPersons  = 21;
    float balanceRatio     = 0.45f;    // counterweight = car + 45% of rated load
    float rotatingMassKg   = 400.0f;   // motor rotor + sheaves, referred to the ropes

    // --- Suspension and compensation ---
    // Above ~30-40 m of travel the weight of the hoist ropes shifts enough load
    // from one side of the sheave to the other that it must be compensated.
    // Chains are used up to ~2.5 m/s; faster lifts use compensating ropes.
    int   hoistRopeCount          = 6;
    float hoistRopeMassPerM       = 0.58f;   // kg/m per 13 mm 8-strand steel rope
    bool  compensated             = true;
    float compensationMassPerM    = 3.48f;   // kg/m of chain, matched to all hoist ropes
    float travelingCableMassPerM  = 1.1f;    // kg/m flat control cable
    float ropeHeadroom            = 4.5f;    // m of rope above the car at the top landing
    float pitLoopDepth            = 1.5f;    // m from the bottom landing down to the chain loop

    // --- Traction machine (gearless PM synchronous, 1:1 roping, single wrap) ---
    float sheaveDiameter    = 0.64f;   // m
    float deflectorDiameter = 0.52f;   // m, spreads the ropes out to the counterweight
    float ropeSpacing       = 1.45f;   // m between the car and counterweight rope lines
    float deflectorDrop     = 2.0f;    // m from sheave center down to deflector center
    float driveEfficiency   = 0.85f;   // motor + inverter
    float grooveFriction    = 0.20f;   // effective friction of an undercut groove

    // --- Doors: center-opening, 1100 mm clear opening ---
    float doorOpenTime     = 1.8f;     // s
    float doorCloseTime    = 2.4f;     // s  closing is slower (kinetic energy limit)
    float doorDwellTime    = 3.0f;     // s  held open when nobody transfers
    float transferTime     = 1.2f;     // s  per passenger boarding or alighting

    // --- Safety devices ---
    // Each device is modelled as a constant average retardation. Which device
    // acts depends on the fault (see Car::UpdateSafety).
    float governorTripRatio = 1.15f;             // EN 81-20: governor trips at >= 115% rated speed
    // Descending overspeed: the governor sets the car's progressive safety gear
    // on the guide rails. EN 81-20 requires 0.2 g to 1.0 g average retardation.
    float safetyGearDecel   = 0.5f * kGravity;
    // Ascending overspeed: the car's safety gear only grips downward, so EN 81-20
    // requires separate ascending car overspeed protection, here a rope brake.
    // Retardation of the empty car must not exceed 1 g; 0.5 g is an assumption.
    float ropeBrakeDecel    = 0.5f * kGravity;
    // Unintended car movement: the detected movement is stopped by the certified
    // machine brake, which must hold the car within 1.2 m of the landing.
    // 0.5 g is an assumption.
    float machineBrakeDecel = 0.5f * kGravity;
    float ucmDetectDistance = 0.15f;             // m moved with doors open before UCM trips (assumption)
    float ucmStopLimit      = 1.2f;              // m, EN 81-20 maximum distance from the landing
    float fullLoadBypass    = 0.80f;             // skip hall calls above 80% load (assumption)

    float CounterweightMassKg() const { return carMassKg + balanceRatio * ratedLoadKg; }
    float HoistRopeMassPerM() const { return hoistRopeCount * hoistRopeMassPerM; }
    // Minimum stroke of an energy-dissipating (oil) buffer, EN 81-20:
    // stopping from 115% of rated speed at an average of 1 g.
    float BufferStroke() const { return 0.0674f * ratedSpeed * ratedSpeed; }
    float SheaveRadius() const { return sheaveDiameter * 0.5f; }
    float DeflectorRadius() const { return deflectorDiameter * 0.5f; }

    // Roping geometry in the vertical plane through the sheave, with the
    // traction sheave center at the origin, +z toward the car, +y up. The car
    // rope drops from the front of the sheave (z = +r) and the counterweight
    // rope drops from the far side of the deflector (z = r - ropeSpacing).
    float DeflectorOffsetZ() const { return SheaveRadius() - ropeSpacing + DeflectorRadius(); }

    // Angle (from +z, counter-clockwise) of the point where the rope coming up
    // from the deflector first touches the traction sheave. The rope runs on
    // the external tangent of the two circles; for tangent points P = C + r*n
    // on both circles, (Cs - Cd) . n = -(r_s - r_d).
    float RopeTangentAngle() const {
        float dz = -DeflectorOffsetZ(), dy = deflectorDrop;           // deflector -> sheave
        float length = std::sqrt(dz * dz + dy * dy);
        float uz = dz / length, uy = dy / length;
        float along = -(SheaveRadius() - DeflectorRadius()) / length;  // n . u
        float across = std::sqrt(1.0f - along * along);                // n . (left normal of u)
        float nz = along * uz - across * uy;
        float ny = along * uy + across * uz;
        return std::atan2(ny, nz);
    }
    // Rope contact arc on the traction sheave: from the tangent point over the
    // top to the car side. A deflector reduces it below 180 degrees.
    float TractionWrapAngle() const { return RopeTangentAngle(); }
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
