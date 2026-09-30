#pragma once

#include "render/InstanceBatch.h"
#include "render/Renderer.h"
#include "view/BuildingLayout.h"
#include "view/SceneBuilder.h"
#include <random>
#include <vector>

// Decorative surroundings, drawn with GPU instancing: the street grid, city
// blocks and towers, street trees and lamps, moving road traffic, and indoor
// plants. Nothing here affects the simulation.
class Environment {
public:
    Environment(const BuildingLayout& layout, const MeshLibrary& meshes);

    void Upload();                 // after the OpenGL context exists
    void Update(float dt);         // drives the road traffic
    void CollectBatches(bool xray, std::vector<const InstanceBatch*>& out) const;
    void AppendRayBoxes(std::vector<RayBox>& boxes) const;   // nearest towers, for reflections
    int  InstanceCount() const;

private:
    struct Vehicle {
        bool      alongX;    // road direction
        float     roadLine;  // z of an east-west road or x of a north-south road
        float     lane;      // signed offset from the centerline; sign = travel direction
        float     position;  // distance along the road
        float     speed;     // m/s
        glm::vec3 color;
    };

    void BuildSiteGrounds();
    void BuildStreets();
    void BuildBlock(float x0, float z0, float x1, float z1);
    void BuildIndoorPlants();
    void SpawnTraffic();
    void RebuildTraffic();

    void AddTree(const glm::vec3& base, float scale);
    void AddPottedPlant(const glm::vec3& base, float scale);
    void AddGround(InstanceBatch& batch, const glm::vec3& min, const glm::vec3& max, const glm::vec3& color);

    const BuildingLayout& m_layout;
    std::mt19937 m_rng{12345};

    // Ground and streets
    InstanceBatch m_sidewalks, m_grass, m_asphalt, m_markings;
    // City
    InstanceBatch m_towers, m_penthouses, m_trunks, m_canopies, m_lampPoles;
    // Traffic (rebuilt every frame)
    InstanceBatch m_carBodies, m_carCabins, m_wheels;
    // Inside the building (hidden in x-ray view)
    InstanceBatch m_pots, m_planters, m_plants, m_indoorTrunks;

    std::vector<Vehicle> m_vehicles;
    std::vector<RayBox> m_rayBoxes;
};
