#include "view/Environment.h"
#include "view/Palette.h"
#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

namespace {

// Street grid: roads every 60 m, 12 m wide. The site block sits between the
// roads at x = +/-30 and z = -43 / +17, with the building's front facing z = +17.
constexpr float kRoadPitch = 60.0f;
constexpr float kRoadWidth = 12.0f;
constexpr float kCityExtent = 300.0f;
constexpr float kFirstRoadX = 30.0f;
constexpr float kFirstRoadZ = 17.0f;
constexpr float kRoadTop = -0.15f;     // 15 cm curb below the sidewalks
// Downtown sits behind the site as seen from the default camera, so the
// skyline forms a backdrop; blocks next to the site stay low-rise.
const glm::vec2 kDowntown{140.0f, -170.0f};

const glm::vec3 kSidewalkColor{0.74f, 0.72f, 0.68f};
const glm::vec3 kPlazaColor{0.66f, 0.64f, 0.60f};
const glm::vec3 kAsphaltColor{0.24f, 0.24f, 0.26f};
const glm::vec3 kPaintColor{0.93f, 0.91f, 0.84f};
const glm::vec3 kTerracotta{0.72f, 0.40f, 0.26f};

std::vector<float> RoadLines(float first) {
    std::vector<float> lines;
    for (float v = first - kRoadPitch * 5.0f; v <= kCityExtent; v += kRoadPitch)
        if (v >= -kCityExtent) lines.push_back(v);
    return lines;
}

}  // namespace

Environment::Environment(const BuildingLayout& layout, const MeshLibrary& meshes)
    : m_layout(layout),
      m_sidewalks(&meshes.cube, Material::Matte, Surface::Concrete),
      m_grass(&meshes.cube, Material::Matte, Surface::Grass),
      m_asphalt(&meshes.cube, Material::Matte, Surface::Asphalt),
      m_markings(&meshes.cube, Material::Matte, Surface::Plain, false),
      m_towers(&meshes.cube, Material::Matte, Surface::Facade),
      m_penthouses(&meshes.cube, Material::Matte, Surface::Concrete),
      m_trunks(&meshes.cylinder, Material::Matte, Surface::Wood),
      m_canopies(&meshes.sphere, Material::Matte, Surface::Foliage),
      m_lampPoles(&meshes.cylinder, Material::Metal, Surface::BrushedMetal),
      m_pots(&meshes.cylinder, Material::Matte, Surface::Concrete),
      m_planters(&meshes.cube, Material::Matte, Surface::Wood),
      m_plants(&meshes.sphere, Material::Matte, Surface::Foliage),
      m_indoorTrunks(&meshes.cylinder, Material::Matte, Surface::Wood) {
    BuildSiteGrounds();
    BuildStreets();
    BuildIndoorPlants();
}

void Environment::AddGround(InstanceBatch& batch, const glm::vec3& min, const glm::vec3& max, const glm::vec3& color) {
    batch.Add(SceneBuilder::BoxTransform((min + max) * 0.5f, max - min), color);
}

// ---------------------------------------------------------------------------
// The building's own block: paving and lawns around the footprint. The
// footprint itself is left open so the elevator pits stay visible.
// ---------------------------------------------------------------------------
void Environment::BuildSiteGrounds() {
    const BuildingLayout& L = m_layout;
    const float W = L.halfWidth + 0.2f;
    const float x0 = -kFirstRoadX + kRoadWidth * 0.5f, x1 = -x0;                 // -24 .. 24
    const float z0 = kFirstRoadZ - kRoadPitch + kRoadWidth * 0.5f;               // -37
    const float z1 = kFirstRoadZ - kRoadWidth * 0.5f;                            //  11
    const float back = L.backZ - 0.3f, walk = 4.0f;

    // Sidewalk ring around the block, including the forecourt in front of the lobby.
    AddGround(m_sidewalks, {x0, -0.6f, L.frontZ}, {x1, 0.0f, z1}, kSidewalkColor);
    AddGround(m_sidewalks, {x0, -0.6f, z0}, {x0 + walk, 0.0f, L.frontZ}, kSidewalkColor);
    AddGround(m_sidewalks, {x1 - walk, -0.6f, z0}, {x1, 0.0f, L.frontZ}, kSidewalkColor);
    AddGround(m_sidewalks, {x0 + walk, -0.6f, z0}, {x1 - walk, 0.0f, z0 + walk}, kSidewalkColor);
    // Lawns beside and behind the building.
    AddGround(m_grass, {x0 + walk, -0.6f, z0 + walk}, {-W, -0.01f, L.frontZ}, palette::kGrass);
    AddGround(m_grass, {W, -0.6f, z0 + walk}, {x1 - walk, -0.01f, L.frontZ}, palette::kGrass);
    AddGround(m_grass, {-W, -0.6f, z0 + walk}, {W, -0.01f, back}, palette::kGrass);

    // Street trees along the front, clear of the entrance.
    for (float x : {-21.0f, -16.0f, 16.0f, 21.0f}) AddTree({x, 0.0f, 9.3f}, 1.0f);
    for (float z : {-30.0f, -20.0f, -10.0f, 0.0f}) {
        AddTree({x0 + 2.0f, 0.0f, z}, 1.0f);
        AddTree({x1 - 2.0f, 0.0f, z}, 1.0f);
    }
    // Lawn trees behind the building.
    for (float x : {-9.0f, 0.0f, 9.0f}) AddTree({x, 0.0f, -22.0f}, 1.3f);
}

// ---------------------------------------------------------------------------
// Roads, markings and every other city block.
// ---------------------------------------------------------------------------
void Environment::BuildStreets() {
    const std::vector<float> xRoads = RoadLines(kFirstRoadX);   // north-south roads at these x
    const std::vector<float> zRoads = RoadLines(kFirstRoadZ);   // east-west roads at these z
    const float half = kRoadWidth * 0.5f;

    // Road surfaces. East-west roads sit 1 mm higher so the two sets never z-fight.
    for (float z : zRoads) AddGround(m_asphalt, {-kCityExtent, -0.75f, z - half}, {kCityExtent, kRoadTop + 0.001f, z + half}, kAsphaltColor);
    for (float x : xRoads) AddGround(m_asphalt, {x - half, -0.75f, -kCityExtent}, {x + half, kRoadTop, kCityExtent}, kAsphaltColor);

    // Dashed center lines, skipping intersections.
    auto nearCrossing = [&](float v, const std::vector<float>& crossings) {
        for (float c : crossings) if (std::abs(v - c) < half + 1.0f) return true;
        return false;
    };
    for (float z : zRoads)
        for (float x = -kCityExtent; x < kCityExtent; x += 6.0f)
            if (!nearCrossing(x, xRoads)) m_markings.Add(SceneBuilder::BoxTransform({x, kRoadTop + 0.01f, z}, {3.0f, 0.02f, 0.15f}), kPaintColor);
    for (float x : xRoads)
        for (float z = -kCityExtent; z < kCityExtent; z += 6.0f)
            if (!nearCrossing(z, zRoads)) m_markings.Add(SceneBuilder::BoxTransform({x, kRoadTop + 0.01f, z}, {0.15f, 0.02f, 3.0f}), kPaintColor);

    // Zebra crossings on all four approaches of every intersection.
    for (float x : xRoads) {
        for (float z : zRoads) {
            for (int stripe = -2; stripe <= 2; ++stripe) {
                float offset = stripe * 1.8f;
                for (float side : {-1.0f, 1.0f}) {
                    m_markings.Add(SceneBuilder::BoxTransform({x + offset, kRoadTop + 0.012f, z + side * (half + 2.0f)}, {0.9f, 0.02f, 3.0f}), kPaintColor);
                    m_markings.Add(SceneBuilder::BoxTransform({x + side * (half + 2.0f), kRoadTop + 0.012f, z + offset}, {3.0f, 0.02f, 0.9f}), kPaintColor);
                }
            }
        }
    }

    // City blocks between the roads (the site block is built separately).
    for (size_t i = 0; i + 1 < xRoads.size(); ++i) {
        for (size_t j = 0; j + 1 < zRoads.size(); ++j) {
            float bx0 = xRoads[i] + half, bx1 = xRoads[i + 1] - half;
            float bz0 = zRoads[j] + half, bz1 = zRoads[j + 1] - half;
            bool isSite = bx0 < 0.0f && bx1 > 0.0f && bz0 < 0.0f && bz1 > 0.0f;
            if (!isSite) BuildBlock(bx0, bz0, bx1, bz1);
        }
    }

    // Reflection targets: the towers nearest the site.
    std::sort(m_rayBoxes.begin(), m_rayBoxes.end(), [](const RayBox& a, const RayBox& b) {
        auto distance = [](const RayBox& r) { return glm::length(glm::vec2((r.box.min.x + r.box.max.x), (r.box.min.z + r.box.max.z))); };
        return distance(a) < distance(b);
    });
    if (m_rayBoxes.size() > 10) m_rayBoxes.resize(10);
}

void Environment::BuildBlock(float x0, float z0, float x1, float z1) {
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    const float walk = 5.0f;
    const glm::vec3 center((x0 + x1) * 0.5f, 0.0f, (z0 + z1) * 0.5f);
    const float distance = glm::length(glm::vec2(center.x, center.z));

    // Sidewalk slab for the whole block, then a lawn or plaza inset from it.
    AddGround(m_sidewalks, {x0, -0.6f, z0}, {x1, 0.0f, z1}, kSidewalkColor);
    bool park = unit(m_rng) < 0.18f;
    glm::vec3 innerMin(x0 + walk, -0.6f, z0 + walk), innerMax(x1 - walk, 0.02f, z1 - walk);
    AddGround(park ? m_grass : m_sidewalks, innerMin, innerMax, park ? palette::kGrass : kPlazaColor);

    // Street trees and lamps along the sidewalk.
    for (float t = 0.2f; t < 0.95f; t += 0.3f) {
        AddTree({x0 + (x1 - x0) * t, 0.0f, z0 + 2.2f}, 0.9f);
        AddTree({x0 + (x1 - x0) * t, 0.0f, z1 - 2.2f}, 0.9f);
        AddTree({x0 + 2.2f, 0.0f, z0 + (z1 - z0) * t}, 0.9f);
        AddTree({x1 - 2.2f, 0.0f, z0 + (z1 - z0) * t}, 0.9f);
    }
    for (float x : {x0 + 1.2f, x1 - 1.2f})
        for (float z : {z0 + 1.2f, z1 - 1.2f})
            m_lampPoles.Add(SceneBuilder::SegmentTransform({x, 0.0f, z}, {x, 6.5f, z}, 0.08f), {0.30f, 0.32f, 0.35f});

    if (park) {
        for (int i = 0; i < 8; ++i)
            AddTree({innerMin.x + 3.0f + unit(m_rng) * (innerMax.x - innerMin.x - 6.0f), 0.02f,
                     innerMin.z + 3.0f + unit(m_rng) * (innerMax.z - innerMin.z - 6.0f)}, 1.2f + unit(m_rng) * 0.6f);
        return;
    }

    // Up to four towers on a 2 x 2 lot grid. Height follows a Gaussian falloff
    // from downtown; the blocks around the site are capped at a few storeys.
    const float downtown = glm::length(glm::vec2(center.x, center.z) - kDowntown);
    const float peak = 25.0f + 110.0f * std::exp(-(downtown * downtown) / (130.0f * 130.0f));
    const float cap = distance < 90.0f ? 32.0f : 150.0f;
    static const glm::vec3 facadeColors[] = {
        {0.78f, 0.74f, 0.68f}, {0.62f, 0.66f, 0.72f}, {0.70f, 0.60f, 0.52f},
        {0.84f, 0.82f, 0.78f}, {0.52f, 0.56f, 0.60f}, {0.66f, 0.52f, 0.44f}};
    const float lotW = (innerMax.x - innerMin.x) * 0.5f, lotD = (innerMax.z - innerMin.z) * 0.5f;
    for (int lot = 0; lot < 4; ++lot) {
        if (unit(m_rng) < 0.3f) continue;   // empty lot
        float w = 9.0f + unit(m_rng) * (lotW - 11.0f);
        float d = 9.0f + unit(m_rng) * (lotD - 11.0f);
        float height = std::clamp(peak * (0.45f + unit(m_rng) * 0.8f), 11.0f, cap);
        height = std::round(height / 3.6f) * 3.6f;                       // whole storeys
        glm::vec3 base(innerMin.x + lotW * (lot % 2 + 0.5f), 0.02f, innerMin.z + lotD * (lot / 2 + 0.5f));
        glm::vec3 color = facadeColors[static_cast<int>(unit(m_rng) * 6.0f) % 6];

        glm::vec3 size(w, height, d);
        m_towers.Add(SceneBuilder::BoxTransform(base + glm::vec3(0.0f, height * 0.5f, 0.0f), size), color);
        m_penthouses.Add(SceneBuilder::BoxTransform(base + glm::vec3(0.0f, height + 1.5f, 0.0f), {w * 0.4f, 3.0f, d * 0.4f}),
                         color * 0.9f);
        m_rayBoxes.push_back({{base - glm::vec3(w, 0.0f, d) * 0.5f, base + glm::vec3(w * 0.5f, height, d * 0.5f)}, color});
    }
}

// ---------------------------------------------------------------------------
// Vegetation
// ---------------------------------------------------------------------------
void Environment::AddTree(const glm::vec3& base, float scale) {
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    float trunkHeight = 2.4f * scale;
    m_trunks.Add(SceneBuilder::SegmentTransform(base, base + glm::vec3(0.0f, trunkHeight, 0.0f), 0.13f * scale),
                 {0.42f, 0.30f, 0.20f});
    glm::vec3 leaves(0.32f + unit(m_rng) * 0.1f, 0.52f + unit(m_rng) * 0.1f, 0.26f);
    // Two overlapping blobs read as a crown.
    m_canopies.Add(SceneBuilder::BoxTransform(base + glm::vec3(0.0f, trunkHeight + 0.8f * scale, 0.0f), glm::vec3(2.6f, 2.2f, 2.6f) * scale),
                   leaves);
    m_canopies.Add(SceneBuilder::BoxTransform(base + glm::vec3(0.4f * scale, trunkHeight + 1.7f * scale, -0.3f * scale),
                                              glm::vec3(1.8f, 1.6f, 1.8f) * scale),
                   leaves * 1.1f);
}

void Environment::AddPottedPlant(const glm::vec3& base, float scale) {
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    m_pots.Add(SceneBuilder::SegmentTransform(base, base + glm::vec3(0.0f, 0.5f * scale, 0.0f), 0.28f * scale), kTerracotta);
    glm::vec3 leaves(0.25f + unit(m_rng) * 0.1f, 0.50f + unit(m_rng) * 0.1f, 0.22f);
    for (int i = 0; i < 3; ++i) {
        float angle = i * 2.094f + unit(m_rng);
        glm::vec3 offset(std::cos(angle) * 0.18f, 0.75f + i * 0.22f, std::sin(angle) * 0.18f);
        m_plants.Add(SceneBuilder::BoxTransform(base + offset * scale, glm::vec3(0.55f, 0.6f, 0.55f) * scale), leaves);
    }
}

void Environment::BuildIndoorPlants() {
    const BuildingLayout& L = m_layout;
    const float bank = L.BankHalfWidth(), W = L.halfWidth;

    for (int f = 1; f < L.spec.floorCount; ++f) {
        float y = L.FloorY(f);
        for (float side : {-1.0f, 1.0f}) {
            AddPottedPlant({side * (bank + 0.9f), y, 0.9f}, 1.0f);    // beside the elevator bank
            AddPottedPlant({side * (W - 1.0f), y, 6.2f}, 1.2f);       // front corners
        }
    }

    // Lobby: timber planters with small trees.
    for (float x : {-10.0f, -4.5f, 4.5f, 10.0f}) {
        glm::vec3 base(x, 0.0f, 5.6f);
        m_planters.Add(SceneBuilder::BoxTransform(base + glm::vec3(0.0f, 0.3f, 0.0f), {1.4f, 0.6f, 1.4f}), {0.55f, 0.38f, 0.24f});
        m_indoorTrunks.Add(SceneBuilder::SegmentTransform(base + glm::vec3(0.0f, 0.6f, 0.0f), base + glm::vec3(0.0f, 2.0f, 0.0f), 0.07f),
                           {0.42f, 0.30f, 0.20f});
        m_plants.Add(SceneBuilder::BoxTransform(base + glm::vec3(0.0f, 2.4f, 0.0f), {1.3f, 1.1f, 1.3f}), {0.30f, 0.55f, 0.28f});
    }
}

// ---------------------------------------------------------------------------
void Environment::Upload() {
    for (InstanceBatch* batch : {&m_sidewalks, &m_grass, &m_asphalt, &m_markings, &m_towers, &m_penthouses,
                                 &m_trunks, &m_canopies, &m_lampPoles,
                                 &m_pots, &m_planters, &m_plants, &m_indoorTrunks})
        batch->Upload();
}

void Environment::CollectBatches(bool xray, std::vector<const InstanceBatch*>& out) const {
    out = {&m_sidewalks, &m_grass, &m_asphalt, &m_markings, &m_towers, &m_penthouses, &m_trunks,
           &m_canopies, &m_lampPoles};
    if (!xray) {
        for (const InstanceBatch* indoor : {&m_pots, &m_planters, &m_plants, &m_indoorTrunks}) out.push_back(indoor);
    }
}

void Environment::AppendRayBoxes(std::vector<RayBox>& boxes) const {
    boxes.insert(boxes.end(), m_rayBoxes.begin(), m_rayBoxes.end());
}

int Environment::InstanceCount() const {
    int total = 0;
    for (const InstanceBatch* batch : {&m_sidewalks, &m_grass, &m_asphalt, &m_markings, &m_towers, &m_penthouses,
                                       &m_trunks, &m_canopies, &m_lampPoles,
                                       &m_pots, &m_planters, &m_plants, &m_indoorTrunks})
        total += batch->Count();
    return total;
}
