#include "ui/Inspector.h"
#include <cmath>
#include <cstdarg>
#include <cstdio>

namespace {

std::string Fmt(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    std::vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    return buffer;
}

const char* PowerMode(float kw) {
    if (kw > 0.5f) return "motoring";
    if (kw < -0.5f) return "regenerating";
    return "idle";
}

}  // namespace

void Inspector::Open(int component) {
    m_active = true;
    m_component = static_cast<Component>((component % ComponentCount + ComponentCount) % ComponentCount);
}

void Inspector::Step(int delta) {
    Open(m_active ? m_component + delta : 0);
}

// ---------------------------------------------------------------------------
// Camera shots. Targets are recomputed every frame so moving parts stay framed.
// ---------------------------------------------------------------------------
Inspector::Shot Inspector::CameraShot(const sim::ElevatorSystem& system, const BuildingLayout& L, int carIndex) const {
    const sim::Car& car = system.Cars()[carIndex];
    const float x = L.ShaftX(carIndex), y = car.Position();
    const float hoistwayMidZ = (L.carCenterZ + L.cwtCenterZ) * 0.5f;

    switch (m_component) {
    case TractionMachine:
        return {L.SheaveCenter(carIndex) + glm::vec3(0.1f, -0.1f, 0.0f), -50.0f, 18.0f, 4.0f, false};
    case HoistRopes:   // side view of the roping plane: sheave, tangent, deflector
        return {{x, L.SheaveCenter(carIndex).y - 1.0f, hoistwayMidZ}, -90.0f, 3.0f, 5.5f, true};
    case Governor:
        return {L.GovernorCenter(carIndex), -60.0f, 15.0f, 2.5f, false};
    case CarAndDoors:
        return {{x, y + 1.2f, L.CarFrontZ() + 0.3f}, -20.0f, 6.0f, 6.5f, false};
    case Counterweight:
        return {{x, L.CounterweightBottomY(y) + L.cwtHeight * 0.5f, L.cwtCenterZ}, 160.0f, 8.0f, 6.5f, true};
    case Compensation:
        return {{x - 0.35f, -0.6f, hoistwayMidZ}, -60.0f, 28.0f, 6.0f, true};
    case Buffers:
        return {{x, -1.2f, hoistwayMidZ}, -110.0f, 20.0f, 4.5f, true};
    case Dispatch:
    default:
        return {{0.0f, 1.6f, 1.5f}, -22.0f, 10.0f, 13.0f, false};
    }
}

// ---------------------------------------------------------------------------
// Information cards
// ---------------------------------------------------------------------------
Inspector::Card Inspector::Describe(const sim::ElevatorSystem& system, const BuildingLayout& L, int carIndex) const {
    const sim::Car& car = system.Cars()[carIndex];
    const sim::ElevatorSpec& spec = system.Spec();
    const sim::TractionState& t = car.Traction();
    const float y = car.Position(), v = car.Velocity();
    const float travel = system.Building().TravelHeight();
    const float deg = 180.0f / sim::kPi;
    Card c;

    switch (m_component) {
    case TractionMachine:
        c.title = "Traction machine and brake";
        c.about = {"Gearless permanent-magnet motor turning the sheave directly.",
                   "Friction in the sheave grooves drives the ropes; the brake shoes",
                   "are red while applied. The yellow mark turns at rope speed."};
        c.live = {Fmt("Sheave  %5.1f rpm   (v / (pi*D) * 60)", t.sheaveRpm),
                  Fmt("Torque  %+6.2f kNm  rope force %+6.2f kN", t.torqueNm / 1000.0f, t.ropeForceN / 1000.0f),
                  Fmt("Power   %+6.1f kW   %s", t.powerKw, PowerMode(t.powerKw)),
                  Fmt("Brake   %s", car.IsRunning() ? "lifted" : "applied")};
        c.equation = "torque = (T_car - T_cwt + m_rot*a) * r      P = F * v";
        c.sources = {"CIBSE-D", "HYMANS"};
        break;

    case HoistRopes: {
        const float rope = spec.HoistRopeMassPerM();
        c.title = "Hoist ropes and traction";
        c.about = {Fmt("%d x 13 mm steel ropes, single wrap with a deflector sheave.", spec.hoistRopeCount),
                   "The ropes hold while the tension ratio stays under the Euler-",
                   Fmt("Eytelwein limit (normal operation). Wrap from geometry: %.0f deg.", spec.TractionWrapAngle() * deg)};
        c.live = {Fmt("T_car %6.1f kN    T_cwt %6.1f kN", t.tensionCarN / 1000.0f, t.tensionCwtN / 1000.0f),
                  Fmt("T1/T2 %.3f   limit %.3f   (%.0f%% used)", t.tensionRatio, t.tractionLimit,
                      100.0f * t.tensionRatio / t.tractionLimit),
                  Fmt("Rope weight: %.0f kg car side, %.0f kg counterweight side",
                      rope * (spec.ropeHeadroom + travel - y), rope * (spec.ropeHeadroom + y))};
        c.equation = Fmt("T1 / T2 <= e^(f * alpha)   f = %.2f, alpha = %.0f deg", spec.grooveFriction,
                         spec.TractionWrapAngle() * deg);
        c.sources = {"HYMANS", "WIEK"};
        break;
    }

    case Governor: {
        const float tripSpeed = spec.governorTripRatio * spec.ratedSpeed;
        const float governorRpm = std::abs(v) / (2.0f * sim::kPi * L.GovernorRadius()) * 60.0f;
        c.title = "Overspeed governor";
        c.about = {"Flyweight governor driven by a rope loop clamped to the car.",
                   "Past 115% of rated speed it trips. Moving down, its rope sets the",
                   "safety gear on the rails; moving up, a rope brake stops the car."};
        c.live = {Fmt("Car speed %5.2f m/s   trip speed %.3f m/s", std::abs(v), tripSpeed),
                  Fmt("Governor sheave %5.0f rpm", governorRpm),
                  car.Safety().governorOk ? "Status: armed  (press O to inject a drive fault)"
                                          : Fmt("Status: TRIPPED - stopped by the %s", sim::ToString(car.StoppedBy()))};
        c.equation = "v_trip >= 1.15 v   down: gear 0.2-1.0 g   up: rope brake <= 1 g";
        c.sources = {"EN81-20", "KONE-EN81", "A17.1"};
        break;
    }

    case CarAndDoors: {
        static const char* doorStates[] = {"closed and locked", "opening", "open", "closing"};
        c.title = "Car and center-opening doors";
        c.about = {"The car door operator drives the landing door through a coupler,",
                   "so landing doors only open where a car is standing. A light",
                   "curtain reverses closing doors; locked doors close the safety chain."};
        c.live = {Fmt("Doors %s, %.0f%% open", doorStates[static_cast<int>(car.Doors().State())],
                      car.Doors().OpenFraction() * 100.0f),
                  Fmt("Light curtain %s   reopenings %d", car.beamBlocked ? "BROKEN" : "clear", car.Doors().ReopenCount()),
                  Fmt("Load %d persons, %.0f of %.0f kg", static_cast<int>(car.riders.size()), car.LoadKg(), spec.ratedLoadKg),
                  Fmt("Last stop error %+.1f mm (limit +/-10 mm)", car.LastStopErrorMm())};
        c.equation = "panel travel s(t) = 3t^2 - 2t^3  (zero speed at both ends)";
        c.sources = {"EN81-20", "A17.1"};
        break;
    }

    case Counterweight:
        c.title = "Counterweight";
        c.about = {"Balances the car plus 45% of rated load, so the motor only lifts",
                   "the difference. It moves opposite to the car; an empty car going",
                   "up is lighter than its counterweight and regenerates energy."};
        c.live = {Fmt("Mass %.0f kg = car %.0f kg + 0.45 x %.0f kg", spec.CounterweightMassKg(), spec.carMassKg, spec.ratedLoadKg),
                  Fmt("Height %5.1f m   speed %+5.2f m/s", L.CounterweightBottomY(y), -v),
                  Fmt("Imbalance %+5.0f kg (car side minus counterweight side)", t.imbalanceKg)};
        c.equation = "m_cwt = m_car + 0.45 * Q        y_cwt = H - y_car";
        c.sources = {"BARNEY", "CIBSE-D"};
        break;

    case Compensation: {
        const float chain = spec.compensated ? spec.compensationMassPerM : 0.0f;
        const float chainImbalance = chain * (2.0f * y - travel);
        c.title = "Compensation chains and traveling cable";
        c.about = {"Over 86 m of travel the hoist ropes shift weight from one side of",
                   "the sheave to the other. Chains under the car and counterweight",
                   "mirror the ropes and cancel it; the loop stays at a fixed height."};
        c.live = {Fmt("Imbalance now          %+5.0f kg", t.imbalanceKg),
                  Fmt("Without the chains     %+5.0f kg", t.imbalanceKg - chainImbalance),
                  Fmt("Chain under car %.0f kg, under counterweight %.0f kg",
                      chain * (spec.pitLoopDepth + y), chain * (spec.pitLoopDepth + travel - y))};
        c.equation = "rope imbalance = rho * (H - 2y), cancelled by chains of equal rho";
        c.sources = {"COMP", "BARNEY"};
        break;
    }

    case Buffers: {
        const float impact = spec.governorTripRatio * spec.ratedSpeed;
        c.title = "Pit buffers";
        c.about = {"Oil buffers under the car and counterweight are the last line of",
                   "defence if a car overruns the bottom landing. The plunger stroke",
                   "is sized to stop from 115% of rated speed at an average of 1 g."};
        c.live = {Fmt("Required stroke %.2f m  (0.0674 x %.1f^2)", spec.BufferStroke(), spec.ratedSpeed),
                  Fmt("Impact speed %.2f m/s (115%% rated)", impact),
                  "Average deceleration <= 1 g; above 2.5 g for < 0.04 s"};
        c.equation = "s >= (1.15 v)^2 / (2 g) = 0.0674 v^2";
        c.sources = {"EN81-20", "OLEO"};
        break;
    }

    case Dispatch:
    default: {
        c.title = "Hall calls and group dispatch";
        c.about = {"Each hall call goes to the car with the lowest estimated time of",
                   "arrival; assignments are reviewed every second. Passengers wait by",
                   "the buttons until a lantern lights. Click a floor to call a car."};
        for (const sim::Car& other : system.Cars())
            c.live.push_back(Fmt("Car %c  ETA to lobby %5.1f s   %s", 'A' + other.Id(),
                                 system.EstimateArrivalTime(other, 0, sim::Direction::Up), sim::ToString(other.mode)));
        c.equation = "ETA = d/v + (stops+1)(v/a + a/j) + stops * t_stop";
        c.sources = {"BARNEY", "CIBSE-D", "DEMAND"};
        break;
    }
    }
    return c;
}
