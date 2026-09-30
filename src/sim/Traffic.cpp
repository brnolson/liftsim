#include "sim/Traffic.h"
#include <cmath>

namespace sim {

const char* ToString(TrafficPattern p) {
    switch (p) {
    case TrafficPattern::UpPeak:     return "Morning up-peak";
    case TrafficPattern::DownPeak:   return "Evening down-peak";
    case TrafficPattern::Lunch:      return "Lunch (two-way)";
    case TrafficPattern::Interfloor: return "Interfloor";
    }
    return "?";
}

TrafficGenerator::TrafficGenerator(int floorCount, unsigned seed)
    : m_floorCount(floorCount), m_rng(seed) {
    m_timeToNext = NextInterval();
}

void TrafficGenerator::Update(float dt, std::vector<TripRequest>& arrivals) {
    if (m_ratePer5Min <= 0.0f) return;
    m_timeToNext -= dt;
    while (m_timeToNext <= 0.0f) {
        arrivals.push_back(RandomTrip());
        m_timeToNext += NextInterval();
    }
}

float TrafficGenerator::NextInterval() {
    float ratePerSecond = m_ratePer5Min / 300.0f;
    std::exponential_distribution<float> interval(ratePerSecond);
    return interval(m_rng);
}

int TrafficGenerator::RandomUpperFloor() {
    std::uniform_int_distribution<int> floor(1, m_floorCount - 1);
    return floor(m_rng);
}

TripRequest TrafficGenerator::RandomTrip() {
    // Share of trips that are [incoming from lobby, outgoing to lobby];
    // the rest are interfloor trips between two upper floors.
    float incoming = 0.0f, outgoing = 0.0f;
    switch (m_pattern) {
    case TrafficPattern::UpPeak:     incoming = 0.85f; outgoing = 0.10f; break;
    case TrafficPattern::DownPeak:   incoming = 0.05f; outgoing = 0.85f; break;
    case TrafficPattern::Lunch:      incoming = 0.40f; outgoing = 0.40f; break;
    case TrafficPattern::Interfloor: incoming = 0.10f; outgoing = 0.10f; break;
    }

    float roll = std::uniform_real_distribution<float>(0.0f, 1.0f)(m_rng);
    if (roll < incoming) return {0, RandomUpperFloor()};
    if (roll < incoming + outgoing) return {RandomUpperFloor(), 0};

    int from = RandomUpperFloor();
    int to = RandomUpperFloor();
    while (to == from) to = RandomUpperFloor();
    return {from, to};
}

}  // namespace sim
