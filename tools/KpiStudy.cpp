// Traffic study: runs the simulation headless for each traffic pattern and
// arrival rate and prints the resulting service KPIs.
// Run:  ./build/liftsim_kpi

#include "sim/ElevatorSystem.h"
#include <cstdio>

using namespace sim;

int main() {
    const float kDt = 1.0f / 120.0f;
    const float kDuration = 30.0f * 60.0f;   // 30 simulated minutes per case

    std::printf("%-18s %8s %9s %9s %11s %8s\n", "Pattern", "Rate/5m", "Avg wait", "Max wait", "Avg journey", "Queued");
    for (TrafficPattern pattern : {TrafficPattern::UpPeak, TrafficPattern::Lunch,
                                   TrafficPattern::Interfloor, TrafficPattern::DownPeak}) {
        for (float rate : {30.0f, 60.0f, 90.0f}) {
            ElevatorSystem system(BuildingSpec{}, ElevatorSpec{}, 5);
            system.Traffic().SetPattern(pattern);
            system.Traffic().SetRate(rate);
            for (float t = 0.0f; t < kDuration; t += kDt) system.Update(kDt);

            const Stats& s = system.GetStats();
            int queued = static_cast<int>(system.Passengers().size()) - s.served;
            std::printf("%-18s %8.0f %8.1fs %8.0fs %10.1fs %8d\n", ToString(pattern), rate,
                        s.AvgWait(), s.maxWait, s.AvgJourney(), queued);
        }
    }
    return 0;
}
