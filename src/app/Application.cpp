#include "app/Application.h"

#include <glad/glad.h>
#include <stb_image_write.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr float kSimStep = 1.0f / 120.0f;    // fixed physics step (s)
constexpr int   kMaxStepsPerFrame = 6000;     // bounds catch-up after a stall

}  // namespace

LaunchOptions ParseOptions(int argc, char* argv[]) {
    LaunchOptions o;
    for (int i = 1; i < argc; ++i) {
        auto next = [&]() { return i + 1 < argc ? argv[++i] : ""; };
        if (!std::strcmp(argv[i], "--screenshot")) o.screenshotPath = next();
        else if (!std::strcmp(argv[i], "--warmup")) o.warmupSeconds = static_cast<float>(std::atof(next()));
        else if (!std::strcmp(argv[i], "--view")) o.view = std::atoi(next());
        else if (!std::strcmp(argv[i], "--inspect")) o.inspect = std::atoi(next());
        else if (!std::strcmp(argv[i], "--car")) o.car = std::atoi(next());
        else if (!std::strcmp(argv[i], "--traffic")) o.traffic = std::atoi(next());
        else if (!std::strcmp(argv[i], "--rate")) o.rate = static_cast<float>(std::atof(next()));
        else if (!std::strcmp(argv[i], "--fault")) o.fault = true;
    }
    return o;
}

Application::Application(const LaunchOptions& options)
    : m_options(options),
      m_system(m_buildingSpec, m_elevatorSpec, 2026),
      m_layout(m_buildingSpec, m_elevatorSpec),
      m_sceneBuilder(m_layout, m_meshes),
      m_environment(m_layout, m_meshes),
      m_people(m_layout) {}

Application::~Application() {
    m_hud.Shutdown();
    m_renderer.Shutdown();
    if (m_context) SDL_GL_DeleteContext(m_context);
    if (m_window) SDL_DestroyWindow(m_window);
    SDL_Quit();
}

bool Application::Init() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) return false;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    m_window = SDL_CreateWindow("LiftSim - Traction Elevator Group Simulator",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, m_width, m_height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!m_window) return false;
    m_context = SDL_GL_CreateContext(m_window);
    if (!m_context || !gladLoadGLLoader(reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress))) return false;
    SDL_GL_GetDrawableSize(m_window, &m_width, &m_height);
    SDL_GL_SetSwapInterval(1);
    std::fprintf(stderr, "OpenGL %s on %s\n", glGetString(GL_VERSION), glGetString(GL_RENDERER));

    m_meshes.Init();
    m_environment.Upload();
    std::fprintf(stderr, "Environment: %d instances\n", m_environment.InstanceCount());
    if (!m_renderer.Init(m_width, m_height) || !m_hud.Init()) {
        std::fprintf(stderr, "Failed to initialise renderer or HUD\n");
        return false;
    }

    m_system.Traffic().SetPattern(static_cast<sim::TrafficPattern>(std::clamp(m_options.traffic, 0, 3)));
    if (m_options.rate >= 0.0f) m_system.Traffic().SetRate(m_options.rate);
    m_state.selectedCar = std::clamp(m_options.car, 0, m_buildingSpec.carCount - 1);
    ApplyView(m_options.view);
    if (m_options.inspect >= 0) {
        m_inspector.Open(m_options.inspect);
        ShowInspectorShot();
    }

    // Screenshots need traffic to look at: run the building for a while first.
    if (!m_options.screenshotPath.empty()) {
        auto simulate = [&](float seconds) {
            for (float t = 0.0f; t < seconds; t += kSimStep) {
                m_system.Update(kSimStep);
                m_people.Update(m_system, kSimStep);
                m_hud.Record(m_system, m_state.selectedCar);
            }
        };
        simulate(m_options.warmupSeconds);
        if (m_options.fault) {
            m_system.InjectDriveFault(m_state.selectedCar);
            simulate(8.0f);
        }
    }
    return true;
}

void Application::Run() {
    const bool screenshotMode = !m_options.screenshotPath.empty();
    Uint64 lastTicks = SDL_GetPerformanceCounter();
    float fpsTimer = 0.0f;
    int fpsFrames = 0, frame = 0;

    while (m_running) {
        Uint64 now = SDL_GetPerformanceCounter();
        float frameDt = std::min(static_cast<float>(now - lastTicks) / SDL_GetPerformanceFrequency(), 0.1f);
        lastTicks = now;

        HandleEvents();
        if (!screenshotMode) StepSimulation(frameDt);
        m_people.Update(m_system, m_simAdvanced);
        m_hud.Record(m_system, m_state.selectedCar);
        UpdateCamera(screenshotMode ? 10.0f : frameDt);   // screenshots snap straight to the target
        RenderFrame();

        if (screenshotMode && ++frame == 45) {             // enough frames for a stable fps reading
            bool ok = SaveScreenshot(m_options.screenshotPath);
            std::fprintf(stderr, "%s %s\n", ok ? "Saved" : "Failed to save", m_options.screenshotPath.c_str());
            m_running = false;
        }
        SDL_GL_SwapWindow(m_window);

        fpsTimer += frameDt;
        if (++fpsFrames == 30) {
            m_state.fps = fpsFrames / fpsTimer;
            fpsFrames = 0;
            fpsTimer = 0.0f;
        }
    }
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------
void Application::HandleEvents() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT:
            m_running = false;
            break;
        case SDL_WINDOWEVENT:
            if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                SDL_GL_GetDrawableSize(m_window, &m_width, &m_height);
                m_renderer.Resize(m_width, m_height);
            }
            break;
        case SDL_MOUSEWHEEL:
            m_camera.Zoom(static_cast<float>(e.wheel.y));
            break;
        case SDL_MOUSEMOTION:
            if (e.motion.state & SDL_BUTTON_LMASK) m_camera.Orbit(-e.motion.xrel * 0.3f, e.motion.yrel * 0.3f);
            if (e.motion.state & SDL_BUTTON_RMASK) {
                m_camera.Pan(static_cast<float>(e.motion.xrel), static_cast<float>(e.motion.yrel));
                m_state.following = false;
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
            m_mouseDownX = e.button.x;
            m_mouseDownY = e.button.y;
            break;
        case SDL_MOUSEBUTTONUP: {
            // A click is a press and release without dragging.
            int moved = std::abs(e.button.x - m_mouseDownX) + std::abs(e.button.y - m_mouseDownY);
            if (e.button.button == SDL_BUTTON_LEFT && moved < 4) HandleClick(e.button.x, e.button.y);
            break;
        }
        case SDL_KEYDOWN:
            HandleKey(e.key);
            break;
        }
    }
}

void Application::HandleKey(const SDL_KeyboardEvent& key) {
    const SDL_Keycode k = key.keysym.sym;
    const int cars = m_buildingSpec.carCount;
    sim::TrafficGenerator& traffic = m_system.Traffic();

    switch (k) {
    case SDLK_ESCAPE:
        if (m_inspector.Active()) m_inspector.Close();
        else if (m_state.showReferences) m_state.showReferences = false;
        else m_running = false;
        break;
    case SDLK_TAB:
        m_inspector.Step((key.keysym.mod & KMOD_SHIFT) ? -1 : 1);
        ShowInspectorShot();
        break;
    case SDLK_i:            m_state.showReferences = !m_state.showReferences; break;
    case SDLK_SPACE:        m_state.paused = !m_state.paused; break;
    case SDLK_RIGHTBRACKET: m_state.simSpeed = std::min(m_state.simSpeed * 2.0f, 32.0f); break;
    case SDLK_LEFTBRACKET:  m_state.simSpeed = std::max(m_state.simSpeed * 0.5f, 0.25f); break;
    case SDLK_f:            m_state.following = !m_state.following; break;
    case SDLK_t:            traffic.SetPattern(static_cast<sim::TrafficPattern>((static_cast<int>(traffic.Pattern()) + 1) % 4)); break;
    case SDLK_EQUALS:
    case SDLK_KP_PLUS:      traffic.SetRate(std::min(traffic.Rate() + 5.0f, 200.0f)); break;
    case SDLK_MINUS:
    case SDLK_KP_MINUS:     traffic.SetRate(std::max(traffic.Rate() - 5.0f, 0.0f)); break;
    case SDLK_o:            m_system.InjectDriveFault(m_state.selectedCar); break;
    case SDLK_r:            m_system.ResetCar(m_state.selectedCar); break;
    case SDLK_x:            m_state.xray = !m_state.xray; break;
    case SDLK_g:            m_state.rayTracing = !m_state.rayTracing; break;
    case SDLK_j:            m_state.shadows = !m_state.shadows; break;
    case SDLK_k:            m_state.outlines = !m_state.outlines; break;
    case SDLK_h:            m_showHud = !m_showHud; break;
    case SDLK_m:            m_state.minimized = !m_state.minimized; break;
    default:
        if (k >= SDLK_1 && k <= SDLK_9 && k - SDLK_1 < cars) {
            m_state.selectedCar = k - SDLK_1;
            if (m_inspector.Active()) ShowInspectorShot();
        } else if (k >= SDLK_F1 && k <= SDLK_F6) {
            m_inspector.Close();
            ApplyView(k - SDLK_F1);
        }
        break;
    }
}

void Application::HandleClick(int mouseX, int mouseY) {
    int windowW, windowH;
    SDL_GetWindowSize(m_window, &windowW, &windowH);

    // HUD source links first (HUD pixels have their origin at the bottom-left).
    float scale = static_cast<float>(m_width) / windowW;
    float hudX = mouseX * scale, hudY = m_height - mouseY * scale;
    if (m_showHud && m_hud.MinimizeButtonAt(hudX, hudY)) {
        m_state.minimized = !m_state.minimized;
        return;
    }
    if (const char* url = m_hud.LinkAt(hudX, hudY)) {
        SDL_OpenURL(url);
        return;
    }

    // Otherwise cast a ray into the scene: the nearest car is selected, the
    // nearest landing gets a new passenger (a hall call).
    Ray ray = m_camera.ScreenRay(static_cast<float>(mouseX), static_cast<float>(mouseY), windowW, windowH);
    float best = 1e9f, t;
    int hitCar = -1, hitFloor = -1;
    for (const sim::Car& car : m_system.Cars()) {
        if (IntersectRayAABB(ray, m_layout.CarBounds(car.Id(), car.Position()), t) && t < best) {
            best = t;
            hitCar = car.Id();
        }
    }
    for (int f = 0; f < m_buildingSpec.floorCount; ++f) {
        AABB landing{{-m_layout.halfWidth, m_layout.FloorY(f) - 0.25f, 0.0f},
                     {m_layout.halfWidth, m_layout.FloorY(f), m_layout.frontZ}};
        if (IntersectRayAABB(ray, landing, t) && t < best) {
            best = t;
            hitCar = -1;
            hitFloor = f;
        }
    }

    if (hitCar >= 0) {
        m_state.selectedCar = hitCar;
    } else if (hitFloor >= 0) {
        int destination = std::uniform_int_distribution<int>(0, m_buildingSpec.floorCount - 2)(m_rng);
        if (destination >= hitFloor) ++destination;   // any floor except this one
        m_system.AddPassenger(hitFloor, destination);
    }
}

// ---------------------------------------------------------------------------
// Simulation and camera
// ---------------------------------------------------------------------------
void Application::StepSimulation(float frameDt) {
    // Fixed time step keeps the physics deterministic and independent of the
    // frame rate; the accumulator carries leftover time to the next frame.
    m_simAdvanced = 0.0f;
    if (m_state.paused) return;
    m_accumulator += frameDt * m_state.simSpeed;
    int steps = 0;
    while (m_accumulator >= kSimStep && steps++ < kMaxStepsPerFrame) {
        m_system.Update(kSimStep);
        m_accumulator -= kSimStep;
        m_simAdvanced += kSimStep;
    }
}

void Application::UpdateCamera(float frameDt) {
    if (m_inspector.Active()) {
        // Keep the inspected component framed while it moves.
        m_camera.SetFollowTarget(m_inspector.CameraShot(m_system, m_layout, m_state.selectedCar).target);
    } else if (m_state.following) {
        const sim::Car& car = m_system.Cars()[m_state.selectedCar];
        m_camera.SetFollowTarget({m_layout.ShaftX(car.Id()), car.Position() + 1.5f, m_layout.carCenterZ});
    }
    m_camera.Update(frameDt);
}

void Application::ApplyView(int view) {
    m_state.xray = false;
    m_state.following = false;
    switch (view) {
    case 1:   // lobby: passengers waiting by the hall buttons
        m_camera.Set({-1.0f, 1.6f, 1.5f}, -22.0f, 10.0f, 13.0f);
        break;
    case 2:   // machine room: traction machines, brakes, governors
        m_camera.Set({0.0f, m_layout.RoofY() + 0.8f, -1.2f}, -35.0f, 38.0f, 15.0f);
        break;
    case 3:   // rear cut-away of the hoistways: counterweights, cars, ropes
        m_state.xray = true;
        m_camera.Set({0.0f, 30.0f, -1.0f}, 145.0f, 6.0f, 62.0f);
        break;
    case 4:   // follow the selected car from behind
        m_state.xray = true;
        m_state.following = true;
        m_camera.Set({m_layout.ShaftX(m_state.selectedCar), 1.5f, m_layout.carCenterZ}, 150.0f, 10.0f, 13.0f);
        break;
    case 5:   // aerial view of the tower in its city block
        m_camera.Set({0.0f, 30.0f, 0.0f}, -30.0f, 28.0f, 190.0f);
        break;
    default:  // three-quarter overview of the lower floors
        m_camera.Set({0.0f, 9.0f, 0.0f}, -38.0f, 14.0f, 38.0f);
        break;
    }
}

void Application::ShowInspectorShot() {
    if (!m_inspector.Active()) return;
    Inspector::Shot shot = m_inspector.CameraShot(m_system, m_layout, m_state.selectedCar);
    m_camera.Set(shot.target, shot.yaw, shot.pitch, shot.distance);
    m_state.xray = shot.xray;
    m_state.following = false;
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------
void Application::RenderFrame() {
    ViewOptions viewOptions{m_camera.Position(), m_state.selectedCar, m_state.xray};
    m_sceneBuilder.Build(m_system, viewOptions, m_items, m_rayBoxes);
    m_people.AppendDrawItems(m_meshes, m_items);
    // People are not added to the reflection rays: one box cannot represent an
    // articulated pose and shows up as a rectangular ghost in mirrors.
    m_environment.AppendRayBoxes(m_rayBoxes);
    m_environment.CollectBatches(m_state.xray, m_batches);

    FrameData frame;
    frame.items = &m_items;
    frame.rayBoxes = &m_rayBoxes;
    frame.batches = &m_batches;
    frame.view = m_camera.View();
    frame.projection = m_camera.Projection(static_cast<float>(m_width) / std::max(m_height, 1));
    frame.cameraPos = m_camera.Position();
    frame.nearPlane = m_camera.Near();
    frame.farPlane = m_camera.Far();
    frame.shadowBounds = m_layout.SceneBounds();
    frame.settings = {m_state.shadows, m_state.rayTracing, m_state.outlines};
    m_renderer.Render(frame);

    m_state.drawCalls = static_cast<int>(m_items.size());
    if (m_showHud)
        m_hud.Draw(m_system, m_layout, m_state, m_inspector, frame.projection * frame.view, m_width, m_height);
}

bool Application::SaveScreenshot(const std::string& path) const {
    std::vector<unsigned char> pixels(static_cast<size_t>(m_width) * m_height * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, m_width, m_height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    stbi_flip_vertically_on_write(1);   // OpenGL's origin is bottom-left
    return stbi_write_png(path.c_str(), m_width, m_height, 3, pixels.data(), m_width * 3) != 0;
}
