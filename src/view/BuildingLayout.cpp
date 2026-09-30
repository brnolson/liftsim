#include "view/BuildingLayout.h"

BuildingLayout::BuildingLayout(const sim::BuildingSpec& s, const sim::ElevatorSpec& e)
    : spec(s), elevator(e), cwtCenterZ(carCenterZ - e.ropeSpacing) {}

float BuildingLayout::ShaftX(int car) const {
    return (car - (spec.carCount - 1) * 0.5f) * shaftPitch;
}

float BuildingLayout::BankHalfWidth() const {
    return spec.carCount * shaftPitch * 0.5f;
}

float BuildingLayout::CounterweightBottomY(float carY) const {
    // Car at the bottom landing <=> counterweight at the top, just clear of
    // the deflector sheave.
    float topPosition = spec.TravelHeight() - 0.6f;
    return topPosition - carY;
}

glm::vec3 BuildingLayout::SheaveCenter(int car) const {
    return {ShaftX(car), RoofY() + 0.9f, carCenterZ - SheaveRadius()};
}

// The deflector sheave sits in the top of the hoistway, below and behind the
// machine, spreading the ropes out to the counterweight (see ElevatorSpec).
glm::vec3 BuildingLayout::DeflectorCenter(int car) const {
    glm::vec3 sheave = SheaveCenter(car);
    return {sheave.x, sheave.y - elevator.deflectorDrop, sheave.z + elevator.DeflectorOffsetZ()};
}

glm::vec3 BuildingLayout::GovernorCenter(int car) const {
    return {ShaftX(car) - carWidth * 0.5f - 0.2f, RoofY() + 0.55f, carCenterZ + 0.45f};
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
            {halfWidth + 1.0f, RoofY() + machineRoomHeight + 1.0f, frontZ + 1.0f}};
}
