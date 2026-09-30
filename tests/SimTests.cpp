// Unit tests for the simulation core. The core has no graphics dependency, so
// it can be verified headless and deterministically (fixed seed, fixed step).
// Run:  ./build/liftsim_tests

#include "sim/ElevatorSystem.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>

using namespace sim;

static int g_failures = 0;
#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::printf("    FAILED: %s  (%s:%d)\n", #cond, __FILE__, __LINE__); \
            ++g_failures;                                                        \
        }                                                                        \
    } while (0)

static constexpr float kDt = 1.0f / 120.0f;

static void Test(const char* name, const std::function<void()>& body) {
    int before = g_failures;
    body();
    std::printf("[%s] %s\n", g_failures == before ? " OK " : "FAIL", name);
}

int main() {
    const ElevatorSpec spec;
    const BuildingSpec building;
    const MotionLimits limits{spec.ratedSpeed, spec.maxAccel, spec.maxJerk,
                              spec.levelingSpeed, spec.stopTolerance};

    Test("Every trip respects speed, acceleration and jerk limits and levels within +/-10 mm", [&] {
        for (int floors = 1; floors < building.floorCount; ++floors) {
            for (int dir : {1, -1}) {
                float start = dir > 0 ? 0.0f : building.TravelHeight();
                float target = start + dir * floors * building.floorHeight;
                MotionController m(limits);
                m.Reset(start);
                m.MoveTo(target);
                float t = 0.0f, maxV = 0.0f, maxA = 0.0f, maxJ = 0.0f, overshoot = 0.0f;
                while (!m.IsStopped() && t < 120.0f) {
                    m.Update(kDt);
                    t += kDt;
                    maxV = std::max(maxV, std::abs(m.Velocity()));
                    maxA = std::max(maxA, std::abs(m.Acceleration()));
                    maxJ = std::max(maxJ, std::abs(m.Jerk()));
                    overshoot = std::max(overshoot, (m.Position() - target) * dir);
                }
                CHECK(m.IsStopped());
                CHECK(maxV <= spec.ratedSpeed + 1e-3f);
                CHECK(maxA <= spec.maxAccel + 1e-3f);
                CHECK(maxJ <= spec.maxJerk + 1e-2f);
                CHECK(std::abs(m.LastStopError()) <= 0.010f);   // EN 81-20 stopping accuracy
                CHECK(overshoot <= 0.001f);
            }
        }
    });

    Test("Dispatcher flight-time estimate is within 25% of the simulated run", [&] {
        for (int floors : {1, 3, 8, 24}) {
            MotionController m(limits);
            m.Reset(0.0f);
            m.MoveTo(floors * building.floorHeight);
            float t = 0.0f;
            while (!m.IsStopped()) { m.Update(kDt); t += kDt; }
            float estimate = MotionController::FlightTime(floors * building.floorHeight, limits);
            CHECK(std::abs(estimate - t) / t < 0.25f);
        }
    });

    Test("Doors reopen when the light curtain is interrupted while closing", [&] {
        DoorOperator doors(spec.doorOpenTime, spec.doorCloseTime);
        doors.Open();
        for (int i = 0; i < 300; ++i) doors.Update(kDt, false);
        CHECK(doors.IsFullyOpen());
        doors.Close();
        for (int i = 0; i < 60; ++i) doors.Update(kDt, false);
        doors.Update(kDt, true);
        CHECK(doors.State() == DoorState::Opening);
        CHECK(doors.ReopenCount() == 1);
    });

    Test("Car refuses to run while the safety chain is open (doors unlocked)", [&] {
        Car car(0, spec, building);
        car.OpenDoors();
        car.Update(kDt);
        CHECK(!car.Safety().Complete());
        CHECK(!car.StartRun(5));
        CHECK(!car.IsRunning());
    });

    Test("Overspeed governor trips at 115% and the safety gear stops the car", [&] {
        Car car(0, spec, building);
        car.InjectDriveFault();
        float peak = 0.0f;
        for (int i = 0; i < 120 * 30 && car.mode != CarMode::OutOfService; ++i) car.Update(kDt);
        CHECK(car.mode == CarMode::OutOfService);
        CHECK(!car.Safety().governorOk);
        for (int i = 0; i < 120 * 5; ++i) { car.Update(kDt); peak = std::max(peak, std::abs(car.Velocity())); }
        CHECK(!car.IsRunning());
        CHECK(peak < 1.2f * spec.ratedSpeed);
    });

    Test("Unintended car movement with doors open is detected and stopped", [&] {
        Car car(0, spec, building);
        car.OpenDoors();
        for (int i = 0; i < 120; ++i) car.Update(kDt);
        car.InjectDriveFault();
        for (int i = 0; i < 120 * 5; ++i) car.Update(kDt);
        CHECK(!car.Safety().ucmOk);
        CHECK(!car.IsRunning());
        CHECK(std::abs(car.Position()) < 0.5f);
    });

    Test("Ropes never slip: tension ratio stays under the traction limit at full load", [&] {
        ElevatorSystem system(building, spec, 7);
        system.Traffic().SetRate(0.0f);
        for (int i = 0; i < spec.capacityPersons; ++i) system.AddPassenger(0, building.floorCount - 1);
        float worstRatio = 0.0f;
        for (int i = 0; i < 120 * 240; ++i) {
            system.Update(kDt);
            for (const Car& c : system.Cars())
                worstRatio = std::max(worstRatio, c.Traction().tensionRatio / c.Traction().tractionLimit);
        }
        CHECK(worstRatio < 1.0f);
    });

    Test("Idle counterweighted car regenerates energy when running up empty", [&] {
        ElevatorSystem system(building, spec, 3);
        system.Traffic().SetRate(0.0f);
        system.AddPassenger(building.floorCount - 1, 0);   // car travels up empty to fetch them
        for (int i = 0; i < 120 * 60; ++i) system.Update(kDt);
        float regen = 0.0f;
        for (const Car& c : system.Cars()) regen += c.EnergyRegenKwh();
        CHECK(regen > 0.0f);
    });

    Test("Every passenger is delivered to the right floor (up-peak, 120 people)", [&] {
        ElevatorSystem system(building, spec, 11);
        system.Traffic().SetRate(0.0f);
        for (int i = 0; i < 120; ++i) system.AddPassenger(0, 1 + i % (building.floorCount - 1));
        for (int i = 0; i < 120 * 60 * 15 && system.GetStats().served < 120; ++i) system.Update(kDt);
        CHECK(system.GetStats().served == 120);
        for (const Passenger& p : system.Passengers()) CHECK(p.state == PassengerState::Arrived);
    });

    Test("Mixed traffic for one simulated hour: nobody stranded, cars never over capacity", [&] {
        ElevatorSystem system(building, spec, 5);
        system.Traffic().SetPattern(TrafficPattern::Lunch);
        system.Traffic().SetRate(90.0f);
        bool overCapacity = false;
        for (int i = 0; i < 120 * 3600; ++i) {
            system.Update(kDt);
            for (const Car& c : system.Cars())
                if (c.LoadKg() > spec.ratedLoadKg + 1e-3f) overCapacity = true;
        }
        system.Traffic().SetRate(0.0f);
        for (int i = 0; i < 120 * 600; ++i) system.Update(kDt);   // drain the building
        CHECK(!overCapacity);
        int stranded = 0;
        for (const Passenger& p : system.Passengers()) stranded += p.state != PassengerState::Arrived;
        CHECK(stranded == 0);
        std::printf("       served %d, avg wait %.1f s, avg journey %.1f s, max wait %.0f s\n",
                    system.GetStats().served, system.GetStats().AvgWait(),
                    system.GetStats().AvgJourney(), system.GetStats().maxWait);
    });

    std::printf("\n%s (%d failure%s)\n", g_failures ? "TESTS FAILED" : "ALL TESTS PASSED",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures ? 1 : 0;
}
