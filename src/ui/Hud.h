#pragma once

#include "sim/ElevatorSystem.h"
#include "ui/Inspector.h"
#include "ui/TextRenderer.h"
#include "view/BuildingLayout.h"
#include <deque>
#include <vector>
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
    bool  showReferences = false;
    bool  minimized = false;   // panels collapsed to a single button
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
              const Inspector& inspector, const glm::mat4& viewProj, int width, int height);

    // URL of the source link under a point in HUD pixels (origin bottom-left), if any.
    const char* LinkAt(float x, float y) const;
    // True if a point in HUD pixels is on the minimize/restore button.
    bool MinimizeButtonAt(float x, float y) const;

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
    void DrawInspectorCard(const Inspector::Card& card, const glm::vec2* anchor, float x, float y);
    void DrawReferences(int width, int height);
    void DrawSourceLink(const char* id, float x, float y, float maxWidth);
    void DrawMinimizeButton(bool minimized, int width);
    void Panel(float x, float y, float w, float h);

    struct Link { float x0, y0, x1, y1; const char* url; };

    TextRenderer m_text;    // monospace, for data
    TextRenderer m_title;   // proportional, for headings
    std::vector<Link> m_links;   // rebuilt every frame
    Link m_minimizeButton{};     // its url is unused
    std::deque<Sample> m_trace;
    int m_traceCar = -1;
    float m_lastSampleTime = -1.0f;
};
