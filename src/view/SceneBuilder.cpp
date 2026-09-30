#include "view/SceneBuilder.h"
#include "view/Palette.h"
#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

using namespace palette;

void MeshLibrary::Init() {
    cube = Mesh::CreateCube(1.0f);
    sphere = Mesh::CreateSphere(0.5f, 20, 12);
    cylinder = Mesh::CreateCylinder(0.5f, 1.0f, 20);
}

// ---------------------------------------------------------------------------
// Transform helpers
// ---------------------------------------------------------------------------
glm::mat4 SceneBuilder::BoxTransform(const glm::vec3& center, const glm::vec3& size) {
    return glm::scale(glm::translate(glm::mat4(1.0f), center), size);
}

// Maps the unit cylinder (along +y) onto the segment a->b.
glm::mat4 SceneBuilder::SegmentTransform(const glm::vec3& a, const glm::vec3& b, float radius) {
    glm::vec3 axis = b - a;
    float length = glm::length(axis);
    glm::vec3 dir = axis / std::max(length, 1e-5f);
    glm::vec3 up(0.0f, 1.0f, 0.0f);

    glm::mat4 rotation(1.0f);
    float cosAngle = glm::dot(up, dir);
    if (cosAngle < -0.9999f) {
        rotation = glm::rotate(glm::mat4(1.0f), glm::pi<float>(), glm::vec3(1, 0, 0));
    } else if (cosAngle < 0.9999f) {
        rotation = glm::rotate(glm::mat4(1.0f), std::acos(cosAngle), glm::normalize(glm::cross(up, dir)));
    }
    glm::mat4 m = glm::translate(glm::mat4(1.0f), (a + b) * 0.5f) * rotation;
    return glm::scale(m, glm::vec3(radius * 2.0f, length, radius * 2.0f));
}

DrawItem SceneBuilder::Box(const glm::vec3& center, const glm::vec3& size, const glm::vec3& color,
                           Material material, Surface surface) const {
    return {&m_meshes.cube, BoxTransform(center, size), color, material, material != Material::Emissive, surface};
}

DrawItem SceneBuilder::Segment(const glm::vec3& a, const glm::vec3& b, float radius, const glm::vec3& color) const {
    return {&m_meshes.cylinder, SegmentTransform(a, b, radius), color, Material::Matte, true};
}

// ---------------------------------------------------------------------------
// Static building
// ---------------------------------------------------------------------------
SceneBuilder::SceneBuilder(const BuildingLayout& layout, const MeshLibrary& meshes)
    : m_layout(layout), m_meshes(meshes) {
    BuildStatic();
}

void SceneBuilder::AddStatic(const glm::vec3& center, const glm::vec3& size, const glm::vec3& color,
                             Material material, Surface surface) {
    m_static.push_back({Box(center, size, color, material, surface), false, {}, {}, m_tagArchitecture});
}

void SceneBuilder::AddWall(const glm::vec3& center, const glm::vec3& size, const glm::vec3& color,
                           const glm::vec3& outward, Material material, Surface surface) {
    glm::vec3 outerFace = center + outward * (0.5f * glm::dot(size, glm::abs(outward)));
    m_static.push_back({Box(center, size, color, material, surface), true, outward, outerFace, m_tagArchitecture});
}

void SceneBuilder::BuildStatic() {
    const BuildingLayout& L = m_layout;
    const float W = L.halfWidth, B = L.BankHalfWidth(), t = L.slabThickness;
    const float roofY = L.RoofY();
    const float hoistwayDepth = -L.backZ;

    for (int f = 0; f < L.spec.floorCount; ++f) {
        float y = L.FloorY(f) - t * 0.5f;
        glm::vec3 corridorColor = (f == 0) ? kLobbyFloor : kCorridor;

        // Slab in front of the elevator bank (landing corridor), polished.
        glm::vec3 frontCenter(0.0f, y, L.frontZ * 0.5f), frontSize(2.0f * W, t, L.frontZ);
        AddStatic(frontCenter, frontSize, corridorColor, Material::Polished, Surface::Terrazzo);
        m_staticRayBoxes.push_back({{frontCenter - frontSize * 0.5f, frontCenter + frontSize * 0.5f}, corridorColor});

        // Slabs beside the hoistways (the hoistways themselves are open shafts).
        m_tagArchitecture = true;
        static const glm::vec3 carpets[] = {{0.42f, 0.47f, 0.55f}, {0.52f, 0.46f, 0.42f}, {0.40f, 0.48f, 0.44f}};
        for (float side : {-1.0f, 1.0f}) {
            AddStatic({side * (W + B) * 0.5f, y, L.backZ * 0.5f}, {W - B, t, hoistwayDepth},
                      f == 0 ? kLobbyFloor : carpets[f % 3], Material::Matte, f == 0 ? Surface::Terrazzo : Surface::Carpet);
        }
        m_tagArchitecture = false;
        // Divider beams between adjacent hoistways, just below the next floor.
        for (int c = 1; c < L.spec.carCount; ++c) {
            float x = L.ShaftX(c) - L.shaftPitch * 0.5f;
            AddStatic({x, L.FloorY(f) + L.spec.floorHeight - 0.4f, L.backZ * 0.5f}, {0.14f, 0.3f, hoistwayDepth}, kSteelDark);
        }
        // Landing door frames: two jambs and a header per entrance.
        m_tagArchitecture = true;
        for (int c = 0; c < L.spec.carCount; ++c) {
            float x = L.ShaftX(c), fy = L.FloorY(f);
            for (float side : {-1.0f, 1.0f}) {
                AddStatic({x + side * (L.doorWidth * 0.5f + 0.07f), fy + L.doorHeight * 0.5f, 0.06f},
                          {0.14f, L.doorHeight, 0.22f}, kStainless, Material::Metal, Surface::BrushedMetal);
            }
            AddStatic({x, fy + L.doorHeight + 0.15f, 0.06f}, {L.doorWidth + 0.42f, 0.3f, 0.26f}, kStainless, Material::Metal, Surface::BrushedMetal);
            AddStatic({x, fy + 0.01f, 0.06f}, {L.doorWidth + 0.1f, 0.02f, 0.2f}, kSteelDark);   // sill
        }
        m_tagArchitecture = false;
        m_tagArchitecture = true;
        AddFloorInterior(f);

        // Curtain wall on both sides: opaque spandrel + reflective glass band.
        float fy = L.FloorY(f);
        for (float side : {-1.0f, 1.0f}) {
            float depth = L.frontZ - L.backZ;
            float zc = (L.frontZ + L.backZ) * 0.5f;
            glm::vec3 outward(side, 0.0f, 0.0f);
            AddWall({side * (W + 0.1f), fy + 0.35f, zc}, {0.2f, 1.2f, depth}, kSpandrel, outward, Material::Matte, Surface::Concrete);
            AddWall({side * (W + 0.1f), fy + 0.95f + (L.spec.floorHeight - 1.2f) * 0.5f, zc},
                    {0.12f, L.spec.floorHeight - 1.2f, depth}, kGlass, outward, Material::Metal);
        }
        m_tagArchitecture = false;
    }

    // Hoistway side walls, back wall, pit floor.
    float wallTop = roofY;
    m_tagArchitecture = true;
    for (float side : {-1.0f, 1.0f}) {
        AddWall({side * (B + 0.1f), (wallTop - L.pitDepth) * 0.5f, L.backZ * 0.5f},
                {0.2f, wallTop + L.pitDepth, hoistwayDepth}, kHoistwayWall, {side, 0.0f, 0.0f}, Material::Matte, Surface::Concrete);
    }
    m_tagArchitecture = false;
    glm::vec3 backCenter(0.0f, (roofY - L.pitDepth) * 0.5f, L.backZ - 0.15f);
    glm::vec3 backSize(2.0f * W + 0.4f, roofY + L.pitDepth, 0.3f);
    m_tagArchitecture = true;
    AddWall(backCenter, backSize, kHoistwayWall, {0.0f, 0.0f, -1.0f}, Material::Matte, Surface::Concrete);
    m_tagArchitecture = false;
    m_staticRayBoxes.push_back({{backCenter - backSize * 0.5f, backCenter + backSize * 0.5f}, kHoistwayWall});
    AddStatic({0.0f, -L.pitDepth - 0.15f, L.backZ * 0.5f}, {2.0f * B, 0.3f, hoistwayDepth}, kSlab, Material::Matte, Surface::Concrete);

    // Car and counterweight guide rails for each hoistway.
    for (int c = 0; c < L.spec.carCount; ++c) {
        float x = L.ShaftX(c);
        float railHeight = roofY + L.pitDepth;
        float railY = (roofY - L.pitDepth) * 0.5f;
        for (float side : {-1.0f, 1.0f}) {
            AddStatic({x + side * L.carRailOffset, railY, L.carCenterZ}, {0.06f, railHeight, 0.12f}, kRail);
            AddStatic({x + side * (L.cwtWidth * 0.5f + 0.1f), railY, L.cwtCenterZ}, {0.05f, railHeight, 0.08f}, kRail);
        }
    }

    // Roof slab, left open above the hoistways so the ropes can be followed down.
    glm::vec3 roofCenter(0.0f, roofY - t * 0.5f, L.frontZ * 0.5f);
    glm::vec3 roofSize(2.0f * W + 0.4f, t, L.frontZ);
    AddStatic(roofCenter, roofSize, kSlab, Material::Matte, Surface::Concrete);
    m_staticRayBoxes.push_back({{roofCenter - roofSize * 0.5f, roofCenter + roofSize * 0.5f}, kSlab});
    m_tagArchitecture = true;
    for (float side : {-1.0f, 1.0f})
        AddStatic({side * (W + B + 0.2f) * 0.5f, roofY - t * 0.5f, L.backZ * 0.5f}, {W - B + 0.2f, t, hoistwayDepth}, kSlab, Material::Matte, Surface::Concrete);
    m_tagArchitecture = false;
    AddStatic({0.0f, roofY + 0.5f, L.frontZ}, {2.0f * W + 0.4f, 1.0f, 0.2f}, kSpandrel, Material::Matte, Surface::Concrete);

    float mrHalf = B + 0.8f, mrH = L.machineRoomHeight, mrFront = 0.8f;
    float mrDepth = mrFront - L.backZ, mrZ = (mrFront + L.backZ) * 0.5f, mrY = roofY + mrH * 0.5f;
    AddWall({0.0f, mrY, L.backZ - 0.1f}, {2.0f * mrHalf, mrH, 0.2f}, kSpandrel, {0, 0, -1}, Material::Matte, Surface::Concrete);
    AddWall({0.0f, mrY, mrFront + 0.1f}, {2.0f * mrHalf, mrH, 0.2f}, kSpandrel, {0, 0, 1}, Material::Matte, Surface::Concrete);
    AddWall({-mrHalf - 0.1f, mrY, mrZ}, {0.2f, mrH, mrDepth}, kSpandrel, {-1, 0, 0}, Material::Matte, Surface::Concrete);
    AddWall({mrHalf + 0.1f, mrY, mrZ}, {0.2f, mrH, mrDepth}, kSpandrel, {1, 0, 0}, Material::Matte, Surface::Concrete);
    AddWall({0.0f, roofY + mrH + 0.1f, mrZ}, {2.0f * mrHalf + 0.4f, 0.2f, mrDepth + 0.4f}, kSlab, {0, 1, 0}, Material::Matte, Surface::Concrete);
    AddStatic({0.0f, roofY + mrH - 0.1f, mrZ}, {0.2f, 0.2f, mrDepth}, kSteelDark);   // hoisting beam

}

void SceneBuilder::AddFloorInterior(int floor) {
    const BuildingLayout& L = m_layout;
    const float y = L.FloorY(floor), W = L.halfWidth, B = L.BankHalfWidth();
    const float ceilingY = y + L.spec.floorHeight - L.slabThickness;

    // Ceiling light strips over the landing corridor (emissive, no shadow).
    for (float z : {1.8f, 4.8f}) AddStatic({0.0f, ceilingY - 0.03f, z}, {9.0f, 0.05f, 0.25f}, kCeilingLight, Material::Emissive);

    // Hall station column between the middle two cars (buttons are dynamic).
    float stationX = L.spec.carCount >= 2 ? (L.ShaftX(L.spec.carCount / 2 - 1) + L.ShaftX(L.spec.carCount / 2)) * 0.5f
                                          : L.ShaftX(0) + L.shaftPitch * 0.5f;
    AddStatic({stationX, y + L.doorHeight * 0.5f + 0.15f, 0.08f}, {0.34f, L.doorHeight + 0.3f, 0.14f}, kStainless, Material::Metal, Surface::BrushedMetal);

    // Office furniture, varied deterministically per floor.
    static const glm::vec3 deskColors[] = {{0.85f, 0.80f, 0.70f}, {0.55f, 0.40f, 0.30f}, {0.90f, 0.90f, 0.92f}};
    glm::vec3 desk = deskColors[floor % 3];
    if (floor == 0) {
        AddStatic({-8.5f, y + 0.55f, 4.2f}, {3.2f, 1.1f, 0.9f}, {0.30f, 0.32f, 0.38f});   // reception desk
        AddStatic({8.0f, y + 0.25f, 5.0f}, {2.4f, 0.5f, 0.7f}, {0.45f, 0.30f, 0.22f}, Material::Matte, Surface::Wood);   // bench
        return;
    }
    for (float side : {-1.0f, 1.0f}) {
        for (float z : {-1.5f, 4.6f}) {
            float x = side * (B + (W - B) * 0.5f + (z > 0 ? 1.5f : 0.0f));
            AddStatic({x, y + 0.72f, z}, {1.8f, 0.06f, 0.9f}, desk, Material::Matte, Surface::Wood);   // desktop
            AddStatic({x, y + 0.36f, z}, {1.6f, 0.7f, 0.08f}, kSteelDark);                  // modesty panel
            AddStatic({x + 0.3f, y + 0.95f, z - 0.25f}, {0.55f, 0.35f, 0.04f}, {0.10f, 0.10f, 0.12f});  // monitor
        }
    }
}

// ---------------------------------------------------------------------------
// Per-frame build
// ---------------------------------------------------------------------------
void SceneBuilder::Build(const sim::ElevatorSystem& system, const ViewOptions& options,
                         std::vector<DrawItem>& items, std::vector<RayBox>& rayBoxes) const {
    items.clear();
    rayBoxes = m_staticRayBoxes;

    for (const StaticItem& s : m_static) {
        if (s.cullable && glm::dot(options.cameraPos - s.point, s.outward) > 0.0f) continue;
        if (s.architecture && options.xray) continue;
        items.push_back(s.item);
    }

    for (const sim::Car& car : system.Cars()) {
        AddCar(car, car.Id() == options.selectedCar, items);
        AddCounterweight(car, items);
        AddMachine(car, items);
        AddTravelingCable(car, items);
        AddPit(car, items);
        rayBoxes.push_back({m_layout.CarBounds(car.Id(), car.Position()), kWoodPanel});
        rayBoxes.push_back({m_layout.CounterweightBounds(car.Id(), car.Position()), kCwtFiller});
    }
    AddLandings(system, options.xray, items);
}

void SceneBuilder::AddCar(const sim::Car& car, bool selected, std::vector<DrawItem>& items) const {
    const BuildingLayout& L = m_layout;
    const float x = L.ShaftX(car.Id()), y = car.Position(), z = L.carCenterZ;
    const float w = L.carWidth, d = L.carDepth, h = L.carHeight;
    const float front = L.CarFrontZ();
    const glm::vec3 accent = CarAccent(car.Id());

    // Car sling: platform, two stiles and the crosshead the ropes hang from.
    items.push_back(Box({x, y - 0.09f, z}, {w + 0.1f, 0.18f, d + 0.05f}, kSteelDark));
    for (float side : {-1.0f, 1.0f}) {
        items.push_back(Box({x + side * (w * 0.5f + 0.09f), y + h * 0.5f + 0.1f, z}, {0.1f, h + 0.6f, 0.14f}, kSteelDark));
        // Guide shoes riding on the rails
        for (float sy : {-0.1f, h + 0.3f})
            items.push_back(Box({x + side * (L.carRailOffset - 0.07f), y + sy, z}, {0.12f, 0.14f, 0.2f}, accent));
    }
    items.push_back(Box({x, y + h + 0.35f, z}, {w + 0.35f, 0.2f, 0.24f}, accent));

    // Cab: wood back wall, stainless side walls, ceiling with a light panel.
    items.push_back(Box({x, y + h * 0.5f, z - d * 0.5f + 0.03f}, {w, h, 0.06f}, kWoodPanel, Material::Matte, Surface::Wood));
    for (float side : {-1.0f, 1.0f})
        items.push_back(Box({x + side * (w * 0.5f - 0.03f), y + h * 0.5f, z}, {0.06f, h, d}, kStainless, Material::Metal, Surface::BrushedMetal));
    items.push_back(Box({x, y + h + 0.04f, z}, {w, 0.08f, d}, kStainless));
    items.push_back(Box({x, y + h - 0.03f, z}, {w - 0.5f, 0.03f, d - 0.5f}, kCeilingLight, Material::Emissive));
    items.push_back(Box({x, y + 0.95f, z - d * 0.5f + 0.1f}, {w - 0.4f, 0.05f, 0.05f}, kStainless, Material::Metal, Surface::BrushedMetal)); // handrail

    // Front: returns beside the entrance and a transom above it.
    float returnWidth = (w - L.doorWidth) * 0.5f;
    for (float side : {-1.0f, 1.0f})
        items.push_back(Box({x + side * (L.doorWidth * 0.5f + returnWidth * 0.5f), y + h * 0.5f, front - 0.03f},
                            {returnWidth, h, 0.06f}, kStainless, Material::Metal, Surface::BrushedMetal));
    items.push_back(Box({x, y + L.doorHeight + (h - L.doorHeight) * 0.5f, front - 0.03f},
                        {L.doorWidth, h - L.doorHeight, 0.06f}, kStainless, Material::Metal, Surface::BrushedMetal));

    // Center-opening car door panels.
    float travel = car.Doors().PanelTravel() * L.doorWidth * 0.5f;
    for (float side : {-1.0f, 1.0f}) {
        float px = x + side * (L.doorWidth * 0.25f + travel);
        items.push_back(Box({px, y + L.doorHeight * 0.5f, front + 0.02f}, {L.doorWidth * 0.5f, L.doorHeight, 0.04f},
                            kStainless, Material::Metal, Surface::BrushedMetal));
    }

    // Floating marker over the selected car; red while out of service.
    if (selected || car.mode == sim::CarMode::OutOfService) {
        glm::vec3 color = car.mode == sim::CarMode::OutOfService ? kFault : accent;
        glm::mat4 m = glm::translate(glm::mat4(1.0f), {x, y + h + 1.1f, z});
        m = glm::rotate(m, glm::radians(45.0f), glm::vec3(0, 0, 1));
        items.push_back({&m_meshes.cube, glm::scale(m, glm::vec3(0.35f, 0.35f, 0.1f)), color, Material::Emissive, false});
    }
}

void SceneBuilder::AddCounterweight(const sim::Car& car, std::vector<DrawItem>& items) const {
    const BuildingLayout& L = m_layout;
    AABB box = L.CounterweightBounds(car.Id(), car.Position());
    glm::vec3 c = (box.min + box.max) * 0.5f, size = box.max - box.min;

    // Steel frame (two uprights + top and bottom beams) holding filler weights.
    for (float side : {-1.0f, 1.0f})
        items.push_back(Box({c.x + side * (size.x * 0.5f - 0.05f), c.y, c.z}, {0.1f, size.y, size.z + 0.04f}, kSteelDark));
    items.push_back(Box({c.x, box.max.y - 0.06f, c.z}, {size.x, 0.12f, size.z + 0.04f}, kSteelDark));
    items.push_back(Box({c.x, box.min.y + 0.06f, c.z}, {size.x, 0.12f, size.z + 0.04f}, kSteelDark));
    // Stack of cast filler plates; the count sets the counterweight mass.
    const int plates = 12;
    float plateHeight = (size.y - 0.24f) / plates;
    for (int i = 0; i < plates; ++i) {
        float py = box.min.y + 0.12f + plateHeight * (i + 0.5f);
        glm::vec3 shade = (i % 2) ? kCwtFiller : kCwtFiller * 0.85f;
        items.push_back(Box({c.x, py, c.z}, {size.x - 0.2f, plateHeight * 0.92f, size.z}, shade, Material::Matte, Surface::Concrete));
    }
}

void SceneBuilder::AddMachine(const sim::Car& car, std::vector<DrawItem>& items) const {
    const BuildingLayout& L = m_layout;
    const sim::TractionState& tr = car.Traction();
    const float roofY = L.RoofY();
    const glm::vec3 sheave = L.SheaveCenter(car.Id());
    const glm::vec3 deflector = L.DeflectorCenter(car.Id());
    const float r = L.SheaveRadius(), rd = L.DeflectorRadius();
    const glm::vec3 xAxis(1, 0, 0);

    // Bedplate, motor, traction sheave (gearless machine, axis along x).
    items.push_back(Box({sheave.x, roofY + 0.2f, sheave.z}, {1.3f, 0.4f, 1.0f}, kMachine));
    items.push_back(Segment(sheave + glm::vec3(0.2f, 0, 0), sheave + glm::vec3(0.7f, 0, 0), 0.42f, kMachine));
    items.push_back(Segment(sheave + glm::vec3(-0.15f, 0, 0), sheave + glm::vec3(0.15f, 0, 0), r, kSteelDark));
    items.push_back(Box({sheave.x, (roofY + 0.4f + sheave.y - 0.3f) * 0.5f, sheave.z}, {0.5f, sheave.y - roofY - 0.7f, 0.5f}, kMachine));

    // Painted index mark on the sheave face shows it turning (also how rope creep is checked).
    auto spinningMark = [&](const glm::vec3& center, float radius, float angle, float faceOffset) {
        glm::mat4 m = glm::translate(glm::mat4(1.0f), center);
        m = glm::rotate(m, -angle, xAxis);
        m = glm::translate(m, {faceOffset, radius * 0.65f, 0.0f});
        items.push_back({&m_meshes.cube, glm::scale(m, glm::vec3(0.03f, radius * 0.5f, 0.07f)), kGovernor, Material::Matte, false});
    };
    spinningMark(sheave, r, tr.sheaveAngle, -0.16f);

    // Brake drum between sheave and motor, gripped by two shoes: red when
    // applied (car stopped), grey when lifted.
    items.push_back(Segment(sheave + glm::vec3(0.15f, 0, 0), sheave + glm::vec3(0.2f, 0, 0), r * 0.85f, kStainless));
    glm::vec3 brakeColor = car.IsRunning() ? kSteelDark : kBrakeSet;
    for (float side : {-1.0f, 1.0f})
        items.push_back(Box(sheave + glm::vec3(0.175f, 0.0f, side * (r * 0.85f + 0.07f)), {0.1f, 0.3f, 0.12f}, brakeColor));

    // Deflector sheave in the top of the hoistway.
    items.push_back(Segment(deflector + glm::vec3(-0.1f, 0, 0), deflector + glm::vec3(0.1f, 0, 0), rd, kSteelDark));
    spinningMark(deflector, rd, tr.sheaveAngle * r / rd, -0.11f);

    // Hoist ropes, three strands. Path from the car up: vertical to the front of
    // the traction sheave, over its top, along the external tangent down to the
    // deflector, over the deflector, then vertically down to the counterweight.
    const float tangent = L.elevator.RopeTangentAngle();
    const float carHitch = car.Position() + L.carHeight + 0.45f;
    const float cwtHitch = L.CounterweightBottomY(car.Position()) + L.cwtHeight;
    auto onCircle = [](const glm::vec3& c, float radius, float angle, float x) {
        return glm::vec3(x, c.y + radius * std::sin(angle), c.z + radius * std::cos(angle));
    };
    auto arc = [&](const glm::vec3& c, float radius, float from, float to, float x) {
        const int segments = 6;
        for (int i = 0; i < segments; ++i) {
            float a0 = from + (to - from) * i / segments, a1 = from + (to - from) * (i + 1) / segments;
            items.push_back(Segment(onCircle(c, radius, a0, x), onCircle(c, radius, a1, x), 0.012f, kRope));
        }
    };
    for (float dx : {-0.07f, 0.0f, 0.07f}) {
        float x = sheave.x + dx;
        items.push_back(Segment(onCircle(sheave, r, 0.0f, x), {x, carHitch, L.carCenterZ}, 0.012f, kRope));
        arc(sheave, r, 0.0f, tangent, x);
        items.push_back(Segment(onCircle(sheave, r, tangent, x), onCircle(deflector, rd, tangent, x), 0.012f, kRope));
        arc(deflector, rd, tangent, sim::kPi, x);
        items.push_back(Segment(onCircle(deflector, rd, sim::kPi, x), {x, cwtHitch, L.cwtCenterZ}, 0.012f, kRope));
    }

    // Overspeed governor: its rope loop is clamped to the car, so the governor
    // sheave spins at car speed. A tension pulley in the pit keeps the loop taut.
    const float govR = L.GovernorRadius();
    const glm::vec3 gov = L.GovernorCenter(car.Id());
    glm::vec3 govColor = car.Safety().governorOk ? kGovernor : kFault;
    items.push_back(Segment(gov + glm::vec3(-0.05f, 0, 0), gov + glm::vec3(0.05f, 0, 0), govR, govColor));
    spinningMark(gov, govR, car.Position() / govR, 0.06f);
    const float pitPulleyY = -L.pitDepth + 0.5f;
    for (float dz : {-govR, govR})
        items.push_back(Segment({gov.x, gov.y, gov.z + dz}, {gov.x, pitPulleyY, gov.z + dz}, 0.008f, kRope));
    items.push_back(Segment({gov.x - 0.04f, pitPulleyY, gov.z}, {gov.x + 0.04f, pitPulleyY, gov.z}, govR, kSteelDark));

    // Controller cabinet with a status lamp.
    glm::vec3 cabinet(sheave.x, roofY + 1.0f, L.backZ + 0.35f);
    items.push_back(Box(cabinet, {0.8f, 2.0f, 0.45f}, {0.82f, 0.82f, 0.80f}));
    glm::vec3 lamp = car.mode == sim::CarMode::OutOfService ? kFault
                   : car.IsRunning() ? kLampUp : kButtonLit;
    items.push_back(Box(cabinet + glm::vec3(0.0f, 0.7f, 0.24f), {0.12f, 0.12f, 0.03f}, lamp, Material::Emissive));
}

void SceneBuilder::AddTravelingCable(const sim::Car& car, std::vector<DrawItem>& items) const {
    // The flat control cable hangs in a U-loop between a fixed point halfway up
    // the hoistway and the underside of the car. Its length is constant, so the
    // bottom of the loop always sits halfway between the two ends minus slack:
    //   yBottom = (yFixed + yCar + pi*r - length) / 2
    const BuildingLayout& L = m_layout;
    const float x = L.ShaftX(car.Id()) + L.carWidth * 0.5f - 0.25f;
    const float r = 0.3f;
    const float zFixed = L.carCenterZ - r, zCar = L.carCenterZ + r;
    const float yFixed = L.spec.TravelHeight() * 0.5f + 1.0f;
    const float yCar = car.Position() - 0.2f;
    const float length = L.spec.TravelHeight() * 0.5f + 3.0f;
    const float yBottom = (yFixed + yCar + sim::kPi * r - length) * 0.5f;
    const glm::vec3 color(0.12f, 0.12f, 0.14f);

    items.push_back(Segment({x, yFixed, zFixed}, {x, yBottom, zFixed}, 0.03f, color));
    items.push_back(Segment({x, yCar, zCar}, {x, yBottom, zCar}, 0.03f, color));
    const int arcSegments = 8;
    for (int i = 0; i < arcSegments; ++i) {
        float a0 = sim::kPi * i / arcSegments, a1 = sim::kPi * (i + 1) / arcSegments;
        glm::vec3 p0(x, yBottom - r * std::sin(a0), L.carCenterZ - r * std::cos(a0));
        glm::vec3 p1(x, yBottom - r * std::sin(a1), L.carCenterZ - r * std::cos(a1));
        items.push_back(Segment(p0, p1, 0.03f, color));
    }
}

void SceneBuilder::AddPit(const sim::Car& car, std::vector<DrawItem>& items) const {
    const BuildingLayout& L = m_layout;
    const sim::ElevatorSpec& spec = L.elevator;
    const float x = L.ShaftX(car.Id());
    const float pitFloor = -L.pitDepth;

    // Compensation chain: hangs from the underside of the car and of the
    // counterweight and joins in a loop in the pit. Car and counterweight move
    // in opposite directions, so the loop stays at a constant height.
    const float chainX = x - 0.35f;
    const float loopRadius = spec.ropeSpacing * 0.5f;
    const glm::vec3 loopCenter(chainX, -spec.pitLoopDepth + loopRadius, (L.carCenterZ + L.cwtCenterZ) * 0.5f);
    const float carBottom = car.Position() - 0.18f;
    const float cwtBottom = L.CounterweightBottomY(car.Position());
    const glm::vec3 chainColor(0.30f, 0.30f, 0.33f);
    if (spec.compensated) {
        items.push_back(Segment({chainX, carBottom, L.carCenterZ}, {chainX, loopCenter.y, L.carCenterZ}, 0.03f, chainColor));
        items.push_back(Segment({chainX, cwtBottom, L.cwtCenterZ}, {chainX, loopCenter.y, L.cwtCenterZ}, 0.03f, chainColor));
        const int segments = 10;
        for (int i = 0; i < segments; ++i) {
            float a0 = sim::kPi * i / segments, a1 = sim::kPi * (i + 1) / segments;
            glm::vec3 p0 = loopCenter + glm::vec3(0.0f, -loopRadius * std::sin(a0), loopRadius * std::cos(a0));
            glm::vec3 p1 = loopCenter + glm::vec3(0.0f, -loopRadius * std::sin(a1), loopRadius * std::cos(a1));
            items.push_back(Segment(p0, p1, 0.03f, chainColor));
        }
    }

    // Oil buffers under the car and counterweight. The plunger extends by the
    // minimum stroke EN 81-20 requires for this rated speed (0.0674 v^2).
    const float stroke = spec.BufferStroke();
    auto buffer = [&](float z, float top) {
        float bodyTop = top - stroke;
        items.push_back(Segment({x, pitFloor, z}, {x, bodyTop, z}, 0.13f, kGovernor));
        items.push_back(Segment({x, bodyTop, z}, {x, top - 0.03f, z}, 0.06f, kStainless));
        items.push_back(Segment({x, top - 0.03f, z}, {x, top, z}, 0.11f, {0.1f, 0.1f, 0.1f}));
    };
    buffer(L.carCenterZ, -0.5f);    // 0.32 m run-by below the car at the bottom landing
    buffer(L.cwtCenterZ, -0.9f);    // 0.30 m run-by below the counterweight at its lowest
}

void SceneBuilder::AddLandings(const sim::ElevatorSystem& system, bool xray, std::vector<DrawItem>& items) const {
    if (xray) return;   // landing fixtures would hide the hoistways
    const BuildingLayout& L = m_layout;
    const int floors = L.spec.floorCount;
    const float stationX = L.spec.carCount >= 2
        ? (L.ShaftX(L.spec.carCount / 2 - 1) + L.ShaftX(L.spec.carCount / 2)) * 0.5f
        : L.ShaftX(0) + L.shaftPitch * 0.5f;

    for (int f = 0; f < floors; ++f) {
        const float y = L.FloorY(f);

        for (const sim::Car& car : system.Cars()) {
            const float x = L.ShaftX(car.Id());
            const bool carHere = std::abs(car.Position() - y) < 0.2f;

            // Landing doors are driven by the car door through a coupler, so
            // they only open when the car is at this landing.
            float travel = carHere ? car.Doors().PanelTravel() * L.doorWidth * 0.5f : 0.0f;
            for (float side : {-1.0f, 1.0f})
                items.push_back(Box({x + side * (L.doorWidth * 0.25f + travel), y + L.doorHeight * 0.5f, 0.02f},
                                    {L.doorWidth * 0.5f, L.doorHeight, 0.04f}, kStainless, Material::Metal, Surface::BrushedMetal));

            // Hall lantern above the entrance announces the arriving car's direction.
            bool announcing = carHere && (car.mode == sim::CarMode::DoorsOpening || car.mode == sim::CarMode::DoorsOpen);
            glm::vec3 lantern = kLampOff;
            Material lanternMaterial = Material::Matte;
            if (announcing && car.direction != sim::Direction::None) {
                lantern = car.direction == sim::Direction::Up ? kLampUp : kLampDown;
                lanternMaterial = Material::Emissive;
            }
            items.push_back(Box({x, y + L.doorHeight + 0.42f, 0.1f}, {0.5f, 0.14f, 0.06f}, lantern, lanternMaterial));
        }

        // Hall call buttons: lit while the call is registered.
        for (sim::Direction d : {sim::Direction::Up, sim::Direction::Down}) {
            if ((d == sim::Direction::Up && f == floors - 1) || (d == sim::Direction::Down && f == 0)) continue;
            bool lit = system.GetHallCall(f, d).active;
            float by = y + 1.1f + (d == sim::Direction::Up ? 0.08f : -0.08f);
            items.push_back(Box({stationX, by, 0.16f}, {0.09f, 0.09f, 0.03f},
                                lit ? kButtonLit : kLampOff, lit ? Material::Emissive : Material::Matte));
        }
    }
}
