#pragma once

#include "render/RayMath.h"
#include "sim/ElevatorSpec.h"
#include <glm/glm.hpp>

// World-space placement of every building element, in meters.
// Axes: +x across the building, +y up, +z toward the open (cut-away) front.
// The landing door line of the elevator bank is the plane z = 0; hoistways
// extend behind it (z < 0) and the landing corridor lies in front (z > 0).
struct BuildingLayout {
    explicit BuildingLayout(const sim::BuildingSpec& spec);

    sim::BuildingSpec spec;

    // Hoistways
    float shaftPitch   = 2.9f;    // center-to-center spacing of adjacent cars
    float carWidth     = 2.0f;
    float carDepth     = 1.5f;
    float carHeight    = 2.5f;
    float carCenterZ   = -0.9f;
    float doorWidth    = 1.1f;    // clear opening; two panels of half this width
    float doorHeight   = 2.1f;
    float cwtWidth     = 1.1f;
    float cwtDepth     = 0.3f;
    float cwtHeight    = 2.6f;
    float cwtCenterZ   = -2.35f;
    float carRailOffset = 1.2f;   // car guide rails at shaftX +/- this
    float pitDepth     = 1.8f;

    // Building shell
    float halfWidth    = 13.0f;
    float backZ        = -2.9f;
    float frontZ       = 7.0f;
    float slabThickness = 0.25f;
    float machineRoomHeight = 3.2f;

    float ShaftX(int car) const;
    float BankHalfWidth() const;              // half-width of the whole hoistway bank
    float FloorY(int floor) const { return spec.FloorY(floor); }
    float RoofY() const { return FloorY(spec.floorCount - 1) + spec.floorHeight; }
    float CarFrontZ() const { return carCenterZ + carDepth * 0.5f; }

    // Counterweight travels opposite to the car (1:1 roping).
    float CounterweightBottomY(float carY) const;
    // Traction sheave: ropes leave its front edge straight down to the car hitch.
    float SheaveRadius() const { return 0.32f; }
    glm::vec3 SheaveCenter(int car) const;
    glm::vec3 DeflectorCenter(int car) const;
    float DeflectorRadius() const;

    AABB CarBounds(int car, float carY) const;
    AABB CounterweightBounds(int car, float carY) const;
    AABB SceneBounds() const;
};
