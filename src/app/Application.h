#pragma once

#include <SDL.h>
#include <random>
#include <string>
#include <vector>

#include "render/OrbitCamera.h"
#include "render/Renderer.h"
#include "sim/ElevatorSystem.h"
#include "ui/Hud.h"
#include "ui/Inspector.h"
#include "view/BuildingLayout.h"
#include "view/Environment.h"
#include "view/PeopleView.h"
#include "view/SceneBuilder.h"

// Command-line options. The screenshot mode renders one frame after a warm-up
// period and exits; it is used to produce the README images.
struct LaunchOptions {
    std::string screenshotPath;
    float warmupSeconds = 120.0f;
    int   view = 0;          // camera preset (F1-F6)
    int   inspect = -1;      // open the component inspector on this component
    int   car = 0;           // selected car
    int   traffic = 0;       // sim::TrafficPattern
    float rate = -1.0f;      // arrivals per 5 minutes; negative keeps the default
    bool  fault = false;     // inject a drive fault on the selected car after warm-up
};

LaunchOptions ParseOptions(int argc, char* argv[]);

// Owns the window and every subsystem, and runs the frame loop:
//   input -> fixed-step simulation -> scene extraction -> render -> HUD
class Application {
public:
    explicit Application(const LaunchOptions& options);
    ~Application();

    bool Init();
    void Run();

private:
    // Frame loop stages
    void HandleEvents();
    void HandleKey(const SDL_KeyboardEvent& key);
    void HandleClick(int mouseX, int mouseY);
    void StepSimulation(float frameDt);
    void UpdateCamera(float frameDt);
    void RenderFrame();

    // Camera presets and the component inspector
    void ApplyView(int view);
    void ShowInspectorShot();
    bool SaveScreenshot(const std::string& path) const;

    LaunchOptions m_options;
    SDL_Window* m_window = nullptr;
    SDL_GLContext m_context = nullptr;
    int m_width = 1600, m_height = 900;

    sim::BuildingSpec m_buildingSpec;
    sim::ElevatorSpec m_elevatorSpec;
    sim::ElevatorSystem m_system;

    MeshLibrary m_meshes;
    BuildingLayout m_layout;
    SceneBuilder m_sceneBuilder;
    Environment m_environment;
    PeopleView m_people;
    Renderer m_renderer;
    Hud m_hud;
    OrbitCamera m_camera;
    Inspector m_inspector;
    HudState m_state;

    std::vector<DrawItem> m_items;
    std::vector<RayBox> m_rayBoxes;
    std::vector<const InstanceBatch*> m_batches;
    std::mt19937 m_rng{7};
    float m_accumulator = 0.0f;
    float m_simAdvanced = 0.0f;   // simulated seconds covered by this frame
    bool  m_running = true;
    bool  m_showHud = true;
    int   m_mouseDownX = 0, m_mouseDownY = 0;
};
