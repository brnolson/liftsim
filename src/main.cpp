// LiftSim: real-time 3D simulation of a traction elevator group.
//
// Frame structure:
//   input -> fixed-step simulation (sim/) -> scene extraction (view/)
//         -> shadow / scene / post passes (render/) -> HUD (ui/)
//
// Command line (used for automated screenshots):
//   LiftSim --screenshot out.png [--warmup 120] [--view 0..4] [--traffic 0..3] [--rate N]

#include <glad/glad.h>
#include <SDL.h>
#include <glm/glm.hpp>
#include <stb_image_write.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#include "render/OrbitCamera.h"
#include "render/Renderer.h"
#include "sim/ElevatorSystem.h"
#include "ui/Hud.h"
#include "view/BuildingLayout.h"
#include "view/PeopleView.h"
#include "view/SceneBuilder.h"

namespace {

constexpr float kSimStep = 1.0f / 120.0f;      // fixed physics step (s)
constexpr int   kMaxStepsPerFrame = 6000;       // bounds catch-up after a stall

struct Options {
    std::string screenshotPath;
    float warmupSeconds = 120.0f;
    int view = 0;
    int traffic = 0;
    float rate = -1.0f;
};

Options ParseOptions(int argc, char* argv[]) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        auto next = [&]() { return i + 1 < argc ? argv[++i] : ""; };
        if (!std::strcmp(argv[i], "--screenshot")) o.screenshotPath = next();
        else if (!std::strcmp(argv[i], "--warmup")) o.warmupSeconds = static_cast<float>(std::atof(next()));
        else if (!std::strcmp(argv[i], "--view")) o.view = std::atoi(next());
        else if (!std::strcmp(argv[i], "--traffic")) o.traffic = std::atoi(next());
        else if (!std::strcmp(argv[i], "--rate")) o.rate = static_cast<float>(std::atof(next()));
    }
    return o;
}

// Camera presets, also used for README screenshots.
void ApplyView(int view, OrbitCamera& camera, const BuildingLayout& layout, HudState& hud) {
    hud.xray = false;
    hud.following = false;
    switch (view) {
    case 1:   // lobby: passengers queueing at the assigned car
        camera.Set({-1.0f, 1.6f, 1.5f}, -22.0f, 10.0f, 13.0f);
        break;
    case 2:   // machine room: traction machines, brakes, governors
        camera.Set({0.0f, layout.RoofY() + 0.8f, -1.2f}, -35.0f, 38.0f, 15.0f);
        break;
    case 3:   // rear cut-away of the hoistways: counterweights, cars, ropes
        hud.xray = true;
        camera.Set({0.0f, 30.0f, -1.0f}, 145.0f, 6.0f, 62.0f);
        break;
    case 4:   // follow the selected car from behind
        hud.xray = true;
        hud.following = true;
        camera.Set({layout.ShaftX(hud.selectedCar), 1.5f, layout.carCenterZ}, 150.0f, 10.0f, 13.0f);
        break;
    default:  // three-quarter overview of the lower floors
        camera.Set({0.0f, 9.0f, 0.0f}, -38.0f, 14.0f, 38.0f);
        break;
    }
}

bool SaveScreenshot(const std::string& path, int width, int height) {
    std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    stbi_flip_vertically_on_write(1);   // OpenGL's origin is bottom-left
    return stbi_write_png(path.c_str(), width, height, 3, pixels.data(), width * 3) != 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    Options options = ParseOptions(argc, argv);
    const bool screenshotMode = !options.screenshotPath.empty();

    if (SDL_Init(SDL_INIT_VIDEO) < 0) return EXIT_FAILURE;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    int width = 1600, height = 900;
    SDL_Window* window = SDL_CreateWindow("LiftSim - Traction Elevator Group Simulator",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window) return EXIT_FAILURE;
    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (!context || !gladLoadGLLoader(reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress))) return EXIT_FAILURE;
    SDL_GL_GetDrawableSize(window, &width, &height);
    SDL_GL_SetSwapInterval(1);
    std::fprintf(stderr, "OpenGL %s on %s\n", glGetString(GL_VERSION), glGetString(GL_RENDERER));

    // --- Simulation ---
    sim::BuildingSpec buildingSpec;
    sim::ElevatorSpec elevatorSpec;
    sim::ElevatorSystem system(buildingSpec, elevatorSpec, 2026);
    system.Traffic().SetPattern(static_cast<sim::TrafficPattern>(std::clamp(options.traffic, 0, 3)));
    if (options.rate >= 0.0f) system.Traffic().SetRate(options.rate);

    // --- View and rendering ---
    MeshLibrary meshes;
    meshes.Init();
    BuildingLayout layout(buildingSpec);
    SceneBuilder sceneBuilder(layout, meshes);
    PeopleView people(layout);
    Renderer renderer;
    Hud hud;
    if (!renderer.Init(width, height) || !hud.Init()) {
        std::fprintf(stderr, "Failed to initialise renderer or HUD\n");
        return EXIT_FAILURE;
    }

    OrbitCamera camera;
    HudState state;
    bool showHud = true;
    ApplyView(options.view, camera, layout, state);

    if (screenshotMode) {   // run the building for a while so there is traffic to see
        for (float t = 0.0f; t < options.warmupSeconds; t += kSimStep) {
            system.Update(kSimStep);
            people.Update(system, kSimStep);
            hud.Record(system, state.selectedCar);
        }
    }

    std::vector<DrawItem> items;
    std::vector<RayBox> rayBoxes;
    std::mt19937 rng(7);
    float accumulator = 0.0f, fpsTimer = 0.0f;
    int fpsFrames = 0, frameCount = 0;
    int mouseDownX = 0, mouseDownY = 0;
    Uint64 lastTicks = SDL_GetPerformanceCounter();
    bool running = true;

    while (running) {
        Uint64 now = SDL_GetPerformanceCounter();
        float frameDt = std::min(static_cast<float>(now - lastTicks) / SDL_GetPerformanceFrequency(), 0.1f);
        lastTicks = now;

        // ------------------------------------------------------------ input
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT: running = false; break;
            case SDL_WINDOWEVENT:
                if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    SDL_GL_GetDrawableSize(window, &width, &height);
                    renderer.Resize(width, height);
                }
                break;
            case SDL_MOUSEWHEEL: camera.Zoom(static_cast<float>(e.wheel.y)); break;
            case SDL_MOUSEMOTION:
                if (e.motion.state & SDL_BUTTON_LMASK) camera.Orbit(-e.motion.xrel * 0.3f, e.motion.yrel * 0.3f);
                if (e.motion.state & SDL_BUTTON_RMASK) { camera.Pan(static_cast<float>(e.motion.xrel), static_cast<float>(e.motion.yrel)); state.following = false; }
                break;
            case SDL_MOUSEBUTTONDOWN: mouseDownX = e.button.x; mouseDownY = e.button.y; break;
            case SDL_MOUSEBUTTONUP: {
                bool click = e.button.button == SDL_BUTTON_LEFT &&
                             std::abs(e.button.x - mouseDownX) + std::abs(e.button.y - mouseDownY) < 4;
                if (!click) break;
                int windowW, windowH;
                SDL_GetWindowSize(window, &windowW, &windowH);
                Ray ray = camera.ScreenRay(static_cast<float>(e.button.x), static_cast<float>(e.button.y), windowW, windowH);

                // Nearest hit among cars (select) and landing floors (call a car).
                float best = 1e9f, t;
                int hitCar = -1, hitFloor = -1;
                for (const sim::Car& car : system.Cars())
                    if (IntersectRayAABB(ray, layout.CarBounds(car.Id(), car.Position()), t) && t < best) { best = t; hitCar = car.Id(); }
                for (int f = 0; f < buildingSpec.floorCount; ++f) {
                    AABB landing{{-layout.halfWidth, layout.FloorY(f) - 0.25f, 0.0f}, {layout.halfWidth, layout.FloorY(f), layout.frontZ}};
                    if (IntersectRayAABB(ray, landing, t) && t < best) { best = t; hitCar = -1; hitFloor = f; }
                }
                if (hitCar >= 0) state.selectedCar = hitCar;
                else if (hitFloor >= 0) {
                    int destination = std::uniform_int_distribution<int>(0, buildingSpec.floorCount - 2)(rng);
                    if (destination >= hitFloor) ++destination;
                    system.AddPassenger(hitFloor, destination);
                }
                break;
            }
            case SDL_KEYDOWN: {
                SDL_Keycode key = e.key.keysym.sym;
                if (key == SDLK_ESCAPE) running = false;
                else if (key == SDLK_SPACE) state.paused = !state.paused;
                else if (key == SDLK_RIGHTBRACKET) state.simSpeed = std::min(state.simSpeed * 2.0f, 32.0f);
                else if (key == SDLK_LEFTBRACKET) state.simSpeed = std::max(state.simSpeed * 0.5f, 0.25f);
                else if (key >= SDLK_1 && key <= SDLK_9 && key - SDLK_1 < buildingSpec.carCount) state.selectedCar = key - SDLK_1;
                else if (key == SDLK_f) state.following = !state.following;
                else if (key == SDLK_t) {
                    auto next = (static_cast<int>(system.Traffic().Pattern()) + 1) % 4;
                    system.Traffic().SetPattern(static_cast<sim::TrafficPattern>(next));
                }
                else if (key == SDLK_EQUALS || key == SDLK_KP_PLUS) system.Traffic().SetRate(std::min(system.Traffic().Rate() + 5.0f, 200.0f));
                else if (key == SDLK_MINUS || key == SDLK_KP_MINUS) system.Traffic().SetRate(std::max(system.Traffic().Rate() - 5.0f, 0.0f));
                else if (key == SDLK_o) system.InjectDriveFault(state.selectedCar);
                else if (key == SDLK_r) system.ResetCar(state.selectedCar);
                else if (key == SDLK_x) state.xray = !state.xray;
                else if (key == SDLK_g) state.rayTracing = !state.rayTracing;
                else if (key == SDLK_j) state.shadows = !state.shadows;
                else if (key == SDLK_k) state.outlines = !state.outlines;
                else if (key == SDLK_h) showHud = !showHud;
                else if (key == SDLK_F1) ApplyView(0, camera, layout, state);
                else if (key == SDLK_F2) ApplyView(1, camera, layout, state);
                else if (key == SDLK_F3) ApplyView(2, camera, layout, state);
                else if (key == SDLK_F4) ApplyView(3, camera, layout, state);
                else if (key == SDLK_F5) ApplyView(4, camera, layout, state);
                break;
            }
            }
        }

        // ------------------------------------------------------- simulation
        // Fixed time step keeps the physics deterministic and independent of
        // frame rate; the accumulator carries leftover time to the next frame.
        float simAdvanced = 0.0f;
        if (!state.paused && !screenshotMode) {
            accumulator += frameDt * state.simSpeed;
            int steps = 0;
            while (accumulator >= kSimStep && steps++ < kMaxStepsPerFrame) {
                system.Update(kSimStep);
                accumulator -= kSimStep;
                simAdvanced += kSimStep;
            }
        }
        people.Update(system, simAdvanced);
        hud.Record(system, state.selectedCar);

        const sim::Car& selected = system.Cars()[state.selectedCar];
        if (state.following)
            camera.SetFollowTarget({layout.ShaftX(selected.Id()), selected.Position() + 1.5f, layout.carCenterZ});
        camera.Update(screenshotMode ? 10.0f : frameDt);   // screenshots snap to the target

        // ----------------------------------------------------------- render
        ViewOptions viewOptions{camera.Position(), state.selectedCar, state.xray};
        sceneBuilder.Build(system, viewOptions, items, rayBoxes);
        people.AppendDrawItems(meshes, items);
        people.AppendRayBoxes(camera.Target(), 20, rayBoxes);

        FrameData frame;
        frame.items = &items;
        frame.rayBoxes = &rayBoxes;
        frame.view = camera.View();
        frame.projection = camera.Projection(static_cast<float>(width) / std::max(height, 1));
        frame.cameraPos = camera.Position();
        frame.nearPlane = camera.Near();
        frame.farPlane = camera.Far();
        frame.shadowBounds = layout.SceneBounds();
        frame.settings = {state.shadows, state.rayTracing, state.outlines};
        renderer.Render(frame);

        state.drawCalls = static_cast<int>(items.size());
        if (showHud) hud.Draw(system, layout, state, frame.projection * frame.view, width, height);

        if (screenshotMode && ++frameCount == 45) {   // enough frames for a stable fps reading
            bool ok = SaveScreenshot(options.screenshotPath, width, height);
            std::fprintf(stderr, "%s %s\n", ok ? "Saved" : "Failed to save", options.screenshotPath.c_str());
            running = false;
        }
        SDL_GL_SwapWindow(window);

        fpsTimer += frameDt;
        if (++fpsFrames == 30) { state.fps = fpsFrames / fpsTimer; fpsFrames = 0; fpsTimer = 0.0f; }
    }

    hud.Shutdown();
    renderer.Shutdown();
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return EXIT_SUCCESS;
}
