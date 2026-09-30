#pragma once

#include "sim/ElevatorSystem.h"
#include "ui/TextRenderer.h"
#include "view/BuildingLayout.h"
#include <deque>
#include <glm/glm.hpp>

struct HudState {
    int   selectedCar = 0;
    float simSpeed = 1.0f;
    bool  paused = false;
    bool  following = false;
    bool  rayTracing = true;
    bool  shadows = true;
    bool  outlines = true;
    bool  xray = false;
    float fps = 0.0f;
    int   drawCalls = 0;
};

// On-screen instrumentation: fleet status table, the selected car's motion
// trace and drive telemetry, safety chain, and traffic KPIs.
class Hud {
public:
    bool Init();
    void Shutdown();

    // Samples the selected car's motion for the speed/acceleration trace.
    void Record(const sim::ElevatorSystem& system, int selectedCar);
    void Draw(const sim::ElevatorSystem& system, const BuildingLayout& layout, const HudState& state,
              const glm::mat4& viewProj, int width, int height);

private:
    struct Sample { float time, speed, accel; };

    void DrawHeader(const sim::ElevatorSystem& system, const HudState& state, float x, float y);
    float DrawFleetTable(const sim::ElevatorSystem& system, const HudState& state, float x, float y);
    void DrawKpis(const sim::ElevatorSystem& system, float x, float y);
    void DrawCarDetail(const sim::Car& car, float x, float y);
    void DrawMotionTrace(float x, float y, float w, float h);
    void DrawControls(int width, float y);
    void DrawWorldLabels(const sim::ElevatorSystem& system, const BuildingLayout& layout,
                         const glm::mat4& viewProj, int width, int height);
    void Panel(float x, float y, float w, float h);

    TextRenderer m_text;    // monospace, for data
    TextRenderer m_title;   // proportional, for headings
    std::deque<Sample> m_trace;
    int m_traceCar = -1;
    float m_lastSampleTime = -1.0f;
};
