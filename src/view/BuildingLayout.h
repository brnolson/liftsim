#pragma once

#include "render/RayMath.h"
#include "sim/ElevatorSpec.h"
#include <glm/glm.hpp>

// World-space placement of every building element, in meters.
// Axes: +x across the building, +y up, +z toward the open (cut-away) front.
// The landing door line of the elevator bank is the plane z = 0; hoistways
// extend behind it (z < 0) and the landing corridor lies in front (z > 0).
struct BuildingLayout {
    BuildingLayout(const sim::BuildingSpec& spec, const sim::ElevatorSpec& elevator);

    sim::BuildingSpec spec;
    sim::ElevatorSpec elevator;

    // Hoistways
    float shaftPitch   = 3.0f;    // center-to-center spacing of adjacent cars
    // Car interior 2.1 x 1.6 m = 3.36 m^2, inside the EN 81-20 maximum of
    // 3.56 m^2 for a 1600 kg rated load (the limit stops a car being overloaded
    // by people simply fitting in).
    float carWidth     = 2.1f;
    float carDepth     = 1.6f;
    float carHeight    = 2.5f;
    float carCenterZ   = -0.9f;
    float doorWidth    = 1.1f;    // clear opening; two panels of half this width
    float doorHeight   = 2.1f;
    float cwtWidth     = 1.1f;
    float cwtDepth     = 0.3f;
    float cwtHeight    = 2.6f;
    float cwtCenterZ;             // derived from the roping geometry
    float carRailOffset = 1.3f;   // car guide rails at shaftX +/- this
    float pitDepth;               // from ElevatorSpec (derived there)

    // Building shell
    float halfWidth    = 13.0f;
    float backZ        = -2.9f;
    float frontZ       = 7.0f;
    float slabThickness = 0.25f;
    float machineRoomHeight = 3.2f;

    float ShaftX(int car) const;
    float BankHalfWidth() const;              // half-width of the whole hoistway bank
    float FloorY(int floor) const { return spec.FloorY(floor); }
    float TopLandingY() const { return FloorY(spec.floorCount - 1); }
    // Roof of the office floors. The hoistways need more overhead than one
    // storey, so the machine room sits on a raised plinth above it.
    float RoofY() const { return TopLandingY() + spec.floorHeight; }
    float MachineRoomFloorY() const { return TopLandingY() + elevator.overheadHeight; }
    float CarFrontZ() const { return carCenterZ + carDepth * 0.5f; }
    // Height of the rope hitch (top of the crosshead) above the car floor.
    float CarHitchHeight() const { return carHeight + 0.45f; }

    // Counterweight travels opposite to the car (1:1 roping).
    float CounterweightBottomY(float carY) const;
    // Top of the oil buffers under the car and the counterweight.
    float CarBufferTopY() const;
    float CounterweightBufferTopY() const;
    // Traction sheave: ropes leave its front edge straight down to the car hitch.
    float SheaveRadius() const { return elevator.SheaveRadius(); }
    float DeflectorRadius() const { return elevator.DeflectorRadius(); }
    float GovernorRadius() const { return 0.16f; }
    glm::vec3 SheaveCenter(int car) const;
    glm::vec3 DeflectorCenter(int car) const;
    glm::vec3 GovernorCenter(int car) const;

    AABB CarBounds(int car, float carY) const;
    AABB CounterweightBounds(int car, float carY) const;
    AABB SceneBounds() const;
};
