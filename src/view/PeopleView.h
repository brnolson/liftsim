#pragma once

#include "render/Renderer.h"
#include "sim/ElevatorSystem.h"
#include "view/BuildingLayout.h"
#include "view/SceneBuilder.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Visual representation of passengers. The simulation only knows *where* a
// passenger logically is (waiting at a floor, riding a car, arrived); this
// class decides where each person stands, walks them there, and animates a
// walk cycle with a small forward-kinematics skeleton.
class PeopleView {
public:
    explicit PeopleView(const BuildingLayout& layout) : m_layout(layout) {}

    void Update(const sim::ElevatorSystem& system, float simDt);
    void AppendDrawItems(const MeshLibrary& meshes, std::vector<DrawItem>& items) const;
    int VisibleCount() const { return static_cast<int>(m_people.size()); }

private:
    struct Look {
        glm::vec3 shirt, pants, skin, hair;
        float height;
    };
    struct Person {
        glm::vec3 position{0.0f};
        glm::vec3 target{0.0f};
        float yaw = 0.0f;            // radians, 0 = facing +z
        float walkPhase = 0.0f;      // radians along the gait cycle
        float stride = 0.0f;         // 0..1, blends between standing and walking
        bool  leaving = false;
        float leaveTimer = 0.0f;
        Look  look;
    };

    static Look RandomLook(int id);
    glm::vec3 SpawnPoint(const sim::Passenger& p) const;
    glm::vec3 ExitPoint(const sim::Passenger& p) const;
    int CarServing(const sim::ElevatorSystem& system, int floor, sim::Direction d) const;

    const BuildingLayout& m_layout;
    std::unordered_map<int, Person> m_people;   // keyed by passenger id
    std::unordered_set<int> m_departed;         // walked out of view; never respawn
};
