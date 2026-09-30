#pragma once

#include "sim/ElevatorSystem.h"
#include "view/BuildingLayout.h"
#include <glm/glm.hpp>
#include <string>
#include <vector>

// Guided close-up tour of the installation. Each component gets a camera
// shot that tracks it while it moves and an information card with live
// readings, the governing equation and its sources.
class Inspector {
public:
    enum Component {
        TractionMachine, HoistRopes, Governor, CarAndDoors, Counterweight,
        Compensation, Buffers, Dispatch, ComponentCount
    };

    struct Shot {
        glm::vec3 target;
        float yaw, pitch, distance;   // degrees, degrees, meters
        bool  xray;
    };

    struct Card {
        std::string title;
        std::vector<std::string> about;   // what it is and what to watch
        std::vector<std::string> live;    // readings from the simulation
        std::string equation;
        std::vector<const char*> sources; // ids into kReferences
    };

    bool Active() const { return m_active; }
    Component Current() const { return m_component; }
    void Open(int component);
    void Step(int delta);             // next / previous component
    void Close() { m_active = false; }

    Shot CameraShot(const sim::ElevatorSystem& system, const BuildingLayout& layout, int car) const;
    Card Describe(const sim::ElevatorSystem& system, const BuildingLayout& layout, int car) const;

private:
    bool m_active = false;
    Component m_component = TractionMachine;
};
