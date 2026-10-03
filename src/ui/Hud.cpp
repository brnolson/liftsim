#include "ui/Hud.h"
#include "ui/References.h"
#include "view/Palette.h"
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <string>

namespace {

const glm::vec3 kWhite{0.95f, 0.96f, 0.98f};
const glm::vec3 kDim{0.62f, 0.66f, 0.72f};
const glm::vec3 kGood{0.35f, 0.95f, 0.55f};
const glm::vec3 kWarn{1.00f, 0.78f, 0.25f};
const glm::vec3 kBad{1.00f, 0.32f, 0.28f};
const glm::vec3 kSpeedLine{0.30f, 0.80f, 1.00f};
const glm::vec3 kAccelLine{1.00f, 0.60f, 0.25f};
const glm::vec3 kLinkColor{0.45f, 0.72f, 1.00f};
constexpr float kLine = 20.0f;          // text line height in pixels
constexpr float kTraceSeconds = 20.0f;

std::string Fmt(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    std::vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    return buffer;
}

}  // namespace

bool Hud::Init() {
    return m_text.Init("C:/Windows/Fonts/consola.ttf", 16.0f) &&
           m_title.Init("C:/Windows/Fonts/segoeuib.ttf", 30.0f);
}

void Hud::Shutdown() {
    m_text.Shutdown();
    m_title.Shutdown();
}

void Hud::Record(const sim::ElevatorSystem& system, int selectedCar) {
    if (selectedCar != m_traceCar) {
        m_trace.clear();
        m_traceCar = selectedCar;
    }
    float t = system.Time();
    if (t - m_lastSampleTime < 1.0f / 30.0f && t >= m_lastSampleTime) return;
    m_lastSampleTime = t;
    const sim::Car& car = system.Cars()[selectedCar];
    m_trace.push_back({t, car.Velocity(), car.Acceleration()});
    while (!m_trace.empty() && t - m_trace.front().time > kTraceSeconds) m_trace.pop_front();
}

void Hud::Panel(float x, float y, float w, float h) {
    m_text.DrawRect(x, y, w, h, {0.05f, 0.07f, 0.10f}, 0.78f);
    m_text.DrawRectOutline(x, y, w, h, 1.0f, {0.35f, 0.40f, 0.48f}, 0.6f);
}

void Hud::Draw(const sim::ElevatorSystem& system, const BuildingLayout& layout, const HudState& state,
               const Inspector& inspector, const glm::mat4& viewProj, int width, int height) {
    m_links.clear();
    m_text.BeginFrame(width, height);
    m_title.BeginFrame(width, height);

    DrawWorldLabels(system, layout, viewProj, width, height);
    DrawMinimizeButton(state.minimized, width);
    if (state.minimized) {
        m_text.EndFrame();
        return;
    }

    float top = static_cast<float>(height) - 16.0f;
    DrawHeader(system, state, 16.0f, top);
    float tableBottom = DrawFleetTable(system, state, 16.0f, top - 118.0f);
    DrawKpis(system, 16.0f, tableBottom - 16.0f);

    if (inspector.Active()) {
        // Leader line from the card to the component on screen.
        glm::vec3 target = inspector.CameraShot(system, layout, state.selectedCar).target;
        glm::vec4 clip = viewProj * glm::vec4(target, 1.0f);
        glm::vec2 anchor((clip.x / clip.w * 0.5f + 0.5f) * width, (clip.y / clip.w * 0.5f + 0.5f) * height);
        bool onScreen = clip.w > 0.0f;
        DrawInspectorCard(inspector.Describe(system, layout, state.selectedCar), onScreen ? &anchor : nullptr,
                          static_cast<float>(width) - 596.0f, top);
    } else {
        DrawCarDetail(system.Cars()[state.selectedCar], static_cast<float>(width) - 436.0f, top);
    }
    if (state.showReferences) DrawReferences(width, height);
    DrawControls(width, 14.0f);

    m_text.EndFrame();
}

const char* Hud::LinkAt(float x, float y) const {
    for (const Link& link : m_links)
        if (x >= link.x0 && x <= link.x1 && y >= link.y0 && y <= link.y1) return link.url;
    return nullptr;
}

bool Hud::MinimizeButtonAt(float x, float y) const {
    const Link& b = m_minimizeButton;
    return x >= b.x0 && x <= b.x1 && y >= b.y0 && y <= b.y1;
}

// Sits just above the controls bar at the bottom-right, clear of every panel.
void Hud::DrawMinimizeButton(bool minimized, int width) {
    const char* label = minimized ? "[+] Show panels (M)" : "[-] Hide panels (M)";
    float w = m_text.MeasureText(label, 0.9f) + 20.0f, h = 26.0f;
    float x = static_cast<float>(width) - w - 16.0f, y = 46.0f;
    Panel(x, y, w, h);
    m_text.DrawText(label, x + 10.0f, y + 8.0f, 0.9f, kWhite);
    m_minimizeButton = {x, y, x + w, y + h, nullptr};
}

void Hud::DrawSourceLink(const char* id, float x, float y, float maxWidth) {
    const Reference* ref = FindReference(id);
    if (!ref) return;
    std::string text = Fmt("[%s] %s", ref->id, ref->citation);
    while (text.size() > 8 && m_text.MeasureText(text, 1.0f) > maxWidth) text = text.substr(0, text.size() - 4) + "...";
    float w = m_text.MeasureText(text, 1.0f);
    m_text.DrawText(text, x, y, 1.0f, kLinkColor);
    m_text.DrawLine(x, y - 3.0f, x + w, y - 3.0f, 1.0f, kLinkColor, 0.6f);
    m_links.push_back({x, y - 5.0f, x + w, y + 14.0f, ref->url});
}

void Hud::DrawInspectorCard(const Inspector::Card& card, const glm::vec2* anchor, float x, float y) {
    const float w = 580.0f;
    const float h = 118.0f + kLine * (card.about.size() + card.live.size() + card.sources.size());
    if (anchor) {
        glm::vec2 from(x, y - 40.0f);
        m_text.DrawLine(from.x, from.y, anchor->x, anchor->y, 2.0f, kWarn, 0.9f);
        m_text.DrawRectOutline(anchor->x - 9.0f, anchor->y - 9.0f, 18.0f, 18.0f, 2.0f, kWarn, 1.0f);
    }
    Panel(x, y - h, w, h);
    m_title.DrawText(card.title, x + 14.0f, y - 34.0f, 0.75f, kWarn);
    m_text.DrawText("Tab / Shift+Tab: next component   Esc: close", x + 14.0f, y - 54.0f, 0.85f, kDim);

    float ly = y - 78.0f;
    for (const std::string& line : card.about) { m_text.DrawText(line, x + 14.0f, ly, 1.0f, kWhite); ly -= kLine; }
    ly -= 6.0f;
    for (const std::string& line : card.live) { m_text.DrawText(line, x + 14.0f, ly, 1.0f, kGood); ly -= kLine; }
    ly -= 6.0f;
    m_text.DrawText(card.equation, x + 14.0f, ly, 1.0f, kSpeedLine);
    ly -= kLine;
    for (const char* id : card.sources) { DrawSourceLink(id, x + 14.0f, ly, w - 28.0f); ly -= kLine; }
}

void Hud::DrawReferences(int width, int height) {
    const int count = static_cast<int>(sizeof(kReferences) / sizeof(kReferences[0]));
    const float w = 900.0f, h = 70.0f + kLine * count;
    const float x = (width - w) * 0.5f, y = height * 0.5f + h * 0.5f;
    Panel(x, y - h, w, h);
    m_title.DrawText("Engineering references", x + 14.0f, y - 34.0f, 0.75f, kWhite);
    m_text.DrawText("click to open   I: close", x + w - 200.0f, y - 30.0f, 0.9f, kDim);
    float ly = y - 58.0f;
    for (const Reference& ref : kReferences) { DrawSourceLink(ref.id, x + 14.0f, ly, w - 28.0f); ly -= kLine; }
}

void Hud::DrawHeader(const sim::ElevatorSystem& system, const HudState& state, float x, float y) {
    Panel(x, y - 108.0f, 560.0f, 108.0f);
    m_title.DrawText("LiftSim", x + 14.0f, y - 36.0f, 1.0f, kWhite);
    m_text.DrawText("Traction elevator group simulator", x + 132.0f, y - 32.0f, 1.0f, kDim);

    int seconds = static_cast<int>(system.Time());
    std::string clock = Fmt("Sim time %02d:%02d:%02d   Speed %gx", seconds / 3600, seconds / 60 % 60,
                            seconds % 60, state.simSpeed);
    m_text.DrawText(clock, x + 14.0f, y - 60.0f, 1.0f, kWhite);
    if (state.paused) m_text.DrawText("PAUSED", x + 330.0f, y - 60.0f, 1.0f, kWarn);
    if (state.following) m_text.DrawText("FOLLOW", x + 400.0f, y - 60.0f, 1.0f, kGood);
    if (state.fps > 0.0f) m_text.DrawText(Fmt("%3.0f fps", state.fps), x + 480.0f, y - 32.0f, 1.0f, kDim);

    const sim::TrafficGenerator& traffic = system.Traffic();
    m_text.DrawText(Fmt("Traffic  %s, %.0f persons / 5 min", sim::ToString(traffic.Pattern()), traffic.Rate()),
                    x + 14.0f, y - 80.0f, 1.0f, kWhite);
    const sim::BuildingSpec& b = system.Building();
    const sim::ElevatorSpec& e = system.Spec();
    m_text.DrawText(Fmt("Building %d floors x %.1f m, %d cars, %.1f m/s, %.0f kg rated", b.floorCount,
                        b.floorHeight, b.carCount, e.ratedSpeed, e.ratedLoadKg),
                    x + 14.0f, y - 100.0f, 1.0f, kDim);
}

float Hud::DrawFleetTable(const sim::ElevatorSystem& system, const HudState& state, float x, float y) {
    const auto& cars = system.Cars();
    float h = 36.0f + kLine * cars.size();
    Panel(x, y - h, 560.0f, h);
    m_text.DrawText("CAR FLOOR DIR  SPEED   ACCEL   LOAD         DOORS       POWER", x + 14.0f, y - 24.0f, 1.0f, kDim);

    float rowY = y - 24.0f - kLine;
    for (const sim::Car& car : cars) {
        bool selected = car.Id() == state.selectedCar;
        if (selected) m_text.DrawRect(x + 4.0f, rowY - 5.0f, 552.0f, kLine, {0.25f, 0.30f, 0.40f}, 0.6f);

        glm::vec3 accent = palette::CarAccent(car.Id());
        m_text.DrawText(std::string(1, palette::CarLetter(car.Id())), x + 18.0f, rowY, 1.0f, accent);

        glm::vec3 color = car.mode == sim::CarMode::OutOfService ? kBad : kWhite;
        std::string row = Fmt("  %3d   %s  %5.2f  %+5.2f  %2d/%-2d %4.0fkg  %-11s %+6.1fkW",
                              car.NearestFloor(), sim::ToString(car.direction), car.Velocity(), car.Acceleration(),
                              static_cast<int>(car.riders.size()), car.Spec().capacityPersons, car.LoadKg(),
                              sim::ToString(car.mode), car.Traction().powerKw);
        m_text.DrawText(row, x + 28.0f, rowY, 1.0f, color);
        rowY -= kLine;
    }
    return y - h;
}

void Hud::DrawKpis(const sim::ElevatorSystem& system, float x, float y) {
    const sim::Stats& s = system.GetStats();
    int waiting = 0;
    for (int f = 0; f < system.Building().floorCount; ++f) waiting += static_cast<int>(system.WaitingAt(f).size());
    float usedKwh = 0.0f, regenKwh = 0.0f;
    for (const sim::Car& car : system.Cars()) {
        usedKwh += car.EnergyUsedKwh();
        regenKwh += car.EnergyRegenKwh();
    }

    Panel(x, y - 104.0f, 560.0f, 104.0f);
    m_text.DrawText("PERFORMANCE", x + 14.0f, y - 22.0f, 1.0f, kDim);
    glm::vec3 waitColor = s.AvgWait() < 30.0f ? kGood : (s.AvgWait() < 50.0f ? kWarn : kBad);
    m_text.DrawText(Fmt("Avg wait %5.1f s", s.AvgWait()), x + 14.0f, y - 44.0f, 1.0f, waitColor);
    m_text.DrawText(Fmt("Max wait %4.0f s   Waits > 60 s  %4.1f%%", s.maxWait, s.PercentLongWaits()),
                    x + 190.0f, y - 44.0f, 1.0f, kWhite);
    m_text.DrawText(Fmt("Avg journey %5.1f s   Served %d   Waiting now %d", s.AvgJourney(), s.served, waiting),
                    x + 14.0f, y - 64.0f, 1.0f, kWhite);
    m_text.DrawText(Fmt("Energy drawn %.2f kWh   Regenerated %.2f kWh", usedKwh, regenKwh),
                    x + 14.0f, y - 84.0f, 1.0f, kWhite);
}

void Hud::DrawCarDetail(const sim::Car& car, float x, float y) {
    const float w = 420.0f, h = 386.0f;
    Panel(x, y - h, w, h);
    glm::vec3 accent = palette::CarAccent(car.Id());
    m_title.DrawText(Fmt("Car %c", palette::CarLetter(car.Id())), x + 14.0f, y - 34.0f, 0.8f, accent);
    glm::vec3 modeColor = car.mode == sim::CarMode::OutOfService ? kBad : kWhite;
    m_text.DrawText(sim::ToString(car.mode), x + 110.0f, y - 30.0f, 1.0f, modeColor);
    m_text.DrawText(Fmt("Floor %d -> %d", car.NearestFloor(), car.TargetFloor()), x + 270.0f, y - 30.0f, 1.0f, kDim);

    DrawMotionTrace(x + 14.0f, y - 190.0f, w - 28.0f, 140.0f);

    const sim::TractionState& t = car.Traction();
    float ly = y - 212.0f;
    m_text.DrawText("DRIVE", x + 14.0f, ly, 1.0f, kDim); ly -= kLine;
    m_text.DrawText(Fmt("Rope force %+6.2f kN   Torque %+6.2f kNm", t.ropeForceN / 1000.0f, t.torqueNm / 1000.0f),
                    x + 14.0f, ly, 1.0f, kWhite); ly -= kLine;
    m_text.DrawText(Fmt("Sheave %5.1f rpm      Power  %+6.1f kW", t.sheaveRpm, t.powerKw),
                    x + 14.0f, ly, 1.0f, t.powerKw < 0.0f ? kGood : kWhite); ly -= kLine;
    float margin = t.tensionRatio / t.tractionLimit;
    m_text.DrawText(Fmt("Traction T1/T2 %.2f of %.2f limit (%2.0f%%)", t.tensionRatio, t.tractionLimit, margin * 100.0f),
                    x + 14.0f, ly, 1.0f, margin < 0.8f ? kWhite : kWarn); ly -= kLine;
    float err = car.LastStopErrorMm();
    m_text.DrawText(Fmt("Last stop error %+5.1f mm (EN 81-20: +/-10)", err), x + 14.0f, ly, 1.0f,
                    std::abs(err) <= 10.0f ? kWhite : kBad);
    ly -= kLine * 1.5f;

    m_text.DrawText("SAFETY CHAIN", x + 14.0f, ly, 1.0f, kDim); ly -= kLine;
    const sim::SafetyChain& s = car.Safety();
    auto contact = [&](const char* name, bool closed, float cx) {
        m_text.DrawRect(cx, ly - 3.0f, 10.0f, 10.0f, closed ? kGood : kBad, 1.0f);
        m_text.DrawText(name, cx + 16.0f, ly - 2.0f, 1.0f, closed ? kWhite : kBad);
    };
    contact("Door locks", s.doorsLocked, x + 14.0f);
    contact("Governor", s.governorOk, x + 150.0f);
    contact("UCM", s.ucmOk, x + 270.0f);
    ly -= kLine;
    std::string chain = "Chain closed: drive may run";
    if (car.StoppedBy() != sim::StoppingDevice::None)
        chain = Fmt("Chain open: stopped by the %s", sim::ToString(car.StoppedBy()));
    else if (!s.Complete())
        chain = "Chain open: drive disabled, brake set";
    m_text.DrawText(chain, x + 14.0f, ly, 1.0f, s.Complete() ? kGood : kWarn);
}

void Hud::DrawMotionTrace(float x, float y, float w, float h) {
    m_text.DrawRect(x, y, w, h, {0.0f, 0.0f, 0.0f}, 0.35f);
    // Axis: speed spans +/-3 m/s, acceleration is drawn on the same scale (m/s^2).
    const float range = 3.0f;
    float midY = y + h * 0.5f;
    m_text.DrawLine(x, midY, x + w, midY, 1.0f, kDim, 0.5f);
    for (float gridValue : {-2.5f, 2.5f}) {
        float gy = midY + gridValue / range * h * 0.5f;
        m_text.DrawLine(x, gy, x + w, gy, 1.0f, kDim, 0.2f);
    }
    m_text.DrawText("v m/s", x + 6.0f, y + h - 16.0f, 1.0f, kSpeedLine);
    m_text.DrawText("a m/s^2", x + 70.0f, y + h - 16.0f, 1.0f, kAccelLine);
    m_text.DrawText("+2.5", x + w - 40.0f, midY + 2.5f / range * h * 0.5f + 3.0f, 1.0f, kDim);
    m_text.DrawText(Fmt("last %.0f s", kTraceSeconds), x + w - 80.0f, y + 4.0f, 1.0f, kDim);
    if (m_trace.size() < 2) return;

    float tEnd = m_trace.back().time;
    auto toScreen = [&](float t, float value) {
        float sx = x + w * (1.0f - (tEnd - t) / kTraceSeconds);
        float sy = midY + std::clamp(value / range, -1.0f, 1.0f) * h * 0.5f;
        return glm::vec2(sx, sy);
    };
    for (size_t i = 1; i < m_trace.size(); ++i) {
        const Sample& a = m_trace[i - 1];
        const Sample& b = m_trace[i];
        glm::vec2 s0 = toScreen(a.time, a.speed), s1 = toScreen(b.time, b.speed);
        glm::vec2 a0 = toScreen(a.time, a.accel), a1 = toScreen(b.time, b.accel);
        m_text.DrawLine(a0.x, a0.y, a1.x, a1.y, 1.5f, kAccelLine);
        m_text.DrawLine(s0.x, s0.y, s1.x, s1.y, 2.0f, kSpeedLine);
    }
}

void Hud::DrawControls(int width, float y) {
    const char* help =
        "LMB drag orbit | RMB drag pan | Wheel zoom | Click car: select, click floor: call | "
        "Tab inspect components | I references | F1-F6 views | 1-9 car | F follow | Space pause | [ ] speed | T traffic | +/- rate | "
        "O drive fault | R reset | X x-ray | G reflections | J shadows | K outlines | M minimize | H hide HUD";
    float w = m_text.MeasureText(help, 0.85f);
    float x = std::max(8.0f, (width - w) * 0.5f);
    m_text.DrawRect(x - 8.0f, y - 6.0f, w + 16.0f, 22.0f, {0.05f, 0.07f, 0.10f}, 0.7f);
    m_text.DrawText(help, x, y, 0.85f, kDim);
}

// Floor numbers and car letters are projected from world space onto the screen.
void Hud::DrawWorldLabels(const sim::ElevatorSystem& system, const BuildingLayout& layout,
                          const glm::mat4& viewProj, int width, int height) {
    auto project = [&](const glm::vec3& p, glm::vec2& out) {
        glm::vec4 clip = viewProj * glm::vec4(p, 1.0f);
        if (clip.w <= 0.1f) return false;
        glm::vec3 ndc = glm::vec3(clip) / clip.w;
        if (std::abs(ndc.x) > 1.0f || std::abs(ndc.y) > 1.0f) return false;
        out = {(ndc.x * 0.5f + 0.5f) * width, (ndc.y * 0.5f + 0.5f) * height};
        return true;
    };

    glm::vec2 s;
    for (int f = 0; f < layout.spec.floorCount; ++f) {
        glm::vec3 anchor(-layout.halfWidth - 0.6f, layout.FloorY(f) + 1.4f, layout.frontZ);
        if (project(anchor, s)) {
            std::string label = f == 0 ? "L" : std::to_string(f);
            float w = m_text.MeasureText(label, 0.9f);
            m_text.DrawText(label, s.x - w, s.y, 0.9f, kWhite);
        }
    }
    for (const sim::Car& car : system.Cars()) {
        glm::vec3 anchor(layout.ShaftX(car.Id()), car.Position() + layout.carHeight + 1.8f, layout.carCenterZ);
        if (project(anchor, s))
            m_title.DrawText(std::string(1, palette::CarLetter(car.Id())), s.x - 7.0f, s.y, 0.6f,
                             palette::CarAccent(car.Id()));
    }
}
