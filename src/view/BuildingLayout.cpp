#include "view/BuildingLayout.h"

BuildingLayout::BuildingLayout(const sim::BuildingSpec& s) : spec(s) {}

float BuildingLayout::ShaftX(int car) const {
    return (car - (spec.carCount - 1) * 0.5f) * shaftPitch;
}

float BuildingLayout::BankHalfWidth() const {
    return spec.carCount * shaftPitch * 0.5f;
}

float BuildingLayout::CounterweightBottomY(float carY) const {
    // Car at the bottom landing <=> counterweight just under the machine room.
    float topPosition = spec.TravelHeight() + 0.4f;
    return topPosition - carY;
}

glm::vec3 BuildingLayout::SheaveCenter(int car) const {
    return {ShaftX(car), RoofY() + 0.9f, carCenterZ - SheaveRadius()};
}

// The deflector sheave spreads the ropes from the traction sheave's back edge
// out to the counterweight, which hangs further back than the sheave diameter.
float BuildingLayout::DeflectorRadius() const {
    float sheaveBackZ = carCenterZ - 2.0f * SheaveRadius();
    return (sheaveBackZ - cwtCenterZ) * 0.5f;
}

glm::vec3 BuildingLayout::DeflectorCenter(int car) const {
    return {ShaftX(car), RoofY() + 0.3f, cwtCenterZ + DeflectorRadius()};
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
