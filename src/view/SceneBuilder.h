#pragma once

#include "render/Renderer.h"
#include "sim/ElevatorSystem.h"
#include "view/BuildingLayout.h"
#include <vector>

// Unit primitives shared by every draw item; model matrices do the rest.
struct MeshLibrary {
    Mesh cube;       // 1 x 1 x 1, centered
    Mesh sphere;     // radius 0.5
    Mesh cylinder;   // radius 0.5, height 1, along +y
    void Init();
};

struct ViewOptions {
    glm::vec3 cameraPos{0.0f};
    int  selectedCar = 0;
    bool xray = false;               // section view: hide architecture, show equipment
};

// Converts simulation state into a flat list of draw items (for rasterizing)
// and a list of boxes (for ray-traced reflections). Static building geometry
// is generated once; moving equipment is regenerated every frame.
class SceneBuilder {
public:
    SceneBuilder(const BuildingLayout& layout, const MeshLibrary& meshes);

    void Build(const sim::ElevatorSystem& system, const ViewOptions& options,
               std::vector<DrawItem>& items, std::vector<RayBox>& rayBoxes) const;

    // Helpers shared with PeopleView.
    static glm::mat4 BoxTransform(const glm::vec3& center, const glm::vec3& size);
    static glm::mat4 SegmentTransform(const glm::vec3& a, const glm::vec3& b, float radius);

private:
    // A static item that is hidden when the camera is on its outer side, so
    // the building always opens toward the viewer (a "dollhouse" cut-away).
    struct StaticItem {
        DrawItem item;
        bool      cullable = false;
        glm::vec3 outward{0.0f};
        glm::vec3 point{0.0f};
        bool      architecture = false;   // hidden in x-ray view
    };

    void BuildStatic();
    void AddStatic(const glm::vec3& center, const glm::vec3& size, const glm::vec3& color,
                   Material material = Material::Matte, Surface surface = Surface::Plain);
    void AddWall(const glm::vec3& center, const glm::vec3& size, const glm::vec3& color,
                 const glm::vec3& outward, Material material = Material::Matte, Surface surface = Surface::Plain);
    void AddFloorInterior(int floor);

    void AddCar(const sim::Car& car, bool selected, std::vector<DrawItem>& items) const;
    void AddCounterweight(const sim::Car& car, std::vector<DrawItem>& items) const;
    void AddMachine(const sim::Car& car, std::vector<DrawItem>& items) const;
    void AddLandings(const sim::ElevatorSystem& system, bool xray, std::vector<DrawItem>& items) const;
    void AddTravelingCable(const sim::Car& car, std::vector<DrawItem>& items) const;
    void AddPit(const sim::Car& car, std::vector<DrawItem>& items) const;

    DrawItem Box(const glm::vec3& center, const glm::vec3& size, const glm::vec3& color,
                 Material material = Material::Matte, Surface surface = Surface::Plain) const;
    DrawItem Segment(const glm::vec3& a, const glm::vec3& b, float radius, const glm::vec3& color) const;

    const BuildingLayout& m_layout;
    const MeshLibrary& m_meshes;
    std::vector<StaticItem> m_static;
    bool m_tagArchitecture = false;   // items added while set are hidden in x-ray view
    std::vector<RayBox> m_staticRayBoxes;
};
