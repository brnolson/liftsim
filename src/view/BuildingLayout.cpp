#include "view/BuildingLayout.h"

BuildingLayout::BuildingLayout(const sim::BuildingSpec& s, const sim::ElevatorSpec& e)
    : spec(s), elevator(e), cwtCenterZ(carCenterZ - e.ropeSpacing), pitDepth(e.pitDepth) {}

float BuildingLayout::ShaftX(int car) const {
    return (car - (spec.carCount - 1) * 0.5f) * shaftPitch;
}

float BuildingLayout::BankHalfWidth() const {
    return spec.carCount * shaftPitch * 0.5f;
}

float BuildingLayout::CounterweightBottomY(float carY) const {
    // 1:1 roping: the counterweight moves exactly opposite the car. Its highest
    // point is reached with the car at the bottom landing. It must stay below
    // the deflector even if the car then over-travels onto its fully compressed
    // buffer (run-by + stroke) and the counterweight jumps (0.035 v^2), plus a
    // 0.1 m margin.
    float deflectorBottom = DeflectorCenter(0).y - DeflectorRadius();
    float clearance = elevator.runBy + elevator.BufferStroke() + elevator.JumpAllowance() + 0.1f;
    float highestBottom = deflectorBottom - clearance - cwtHeight;
    return highestBottom - carY;
}

// Buffers sit one run-by below what they stop at the end of travel: the car
// platform at the bottom landing, and the counterweight with the car at the top.
float BuildingLayout::CarBufferTopY() const {
    const float platformDepth = 0.18f;   // car floor to underside of the platform
    return -platformDepth - elevator.runBy;
}

float BuildingLayout::CounterweightBufferTopY() const {
    return CounterweightBottomY(spec.TravelHeight()) - elevator.runBy;
}

// The sheave is placed so the car rope from the crosshead hitch to the sheave
// centre is exactly ElevatorSpec::ropeHeadroom at the top landing. The rope
// weight in the physics and the drawn rope therefore match.
glm::vec3 BuildingLayout::SheaveCenter(int car) const {
    float y = TopLandingY() + CarHitchHeight() + elevator.ropeHeadroom;
    return {ShaftX(car), y, carCenterZ - SheaveRadius()};
}

// The deflector sheave sits in the top of the hoistway, below and behind the
// machine, spreading the ropes out to the counterweight (see ElevatorSpec).
glm::vec3 BuildingLayout::DeflectorCenter(int car) const {
    glm::vec3 sheave = SheaveCenter(car);
    return {sheave.x, sheave.y - elevator.deflectorDrop, sheave.z + elevator.DeflectorOffsetZ()};
}

// The governor stands in the machine room beside the hoistway, between the car
// guide rail and the divider beam, so its rope loop can hang straight down
// past the car to the tension pulley in the pit.
glm::vec3 BuildingLayout::GovernorCenter(int car) const {
    return {ShaftX(car) - carRailOffset - 0.08f, MachineRoomFloorY() + 0.55f, carCenterZ + 0.45f};
}

AABB BuildingLayout::CarBounds(int car, float carY) const {
    float x = ShaftX(car);
    return {{x - carWidth * 0.5f, carY - 0.18f, carCenterZ - carDepth * 0.5f},
            {x + carWidth * 0.5f, carY + carHeight + 0.1f, carCenterZ + carDepth * 0.5f}};
}

AABB BuildingLayout::CounterweightBounds(int car, float carY) const {
    float x = ShaftX(car), y = CounterweightBottomY(carY);
    return {{x - cwtWidth * 0.5f, y, cwtCenterZ - cwtDepth * 0.5f},
            {x + cwtWidth * 0.5f, y + cwtHeight, cwtCenterZ + cwtDepth * 0.5f}};
}

AABB BuildingLayout::SceneBounds() const {
    return {{-halfWidth - 1.0f, -pitDepth, backZ - 1.0f},
            {halfWidth + 1.0f, MachineRoomFloorY() + machineRoomHeight + 1.0f, frontZ + 1.0f}};
}
