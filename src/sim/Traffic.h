#pragma once

#include <random>
#include <vector>

namespace sim {

// Classic office-building traffic patterns (Barney & Al-Sharif, CIBSE Guide D).
enum class TrafficPattern { UpPeak, DownPeak, Lunch, Interfloor };

const char* ToString(TrafficPattern p);

struct TripRequest {
    int origin;
    int destination;
};

// Generates passenger arrivals as a Poisson process: the time between two
// arrivals is exponentially distributed with mean 1/rate.
class TrafficGenerator {
public:
    TrafficGenerator(int floorCount, unsigned seed);

    void SetPattern(TrafficPattern p) { m_pattern = p; }
    void SetRate(float personsPerFiveMinutes) { m_ratePer5Min = personsPerFiveMinutes; }
    TrafficPattern Pattern() const { return m_pattern; }
    float Rate() const { return m_ratePer5Min; }

    // Appends every arrival that happens during this time step.
    void Update(float dt, std::vector<TripRequest>& arrivals);

private:
    TripRequest RandomTrip();
    int RandomUpperFloor();
    float NextInterval();

    int m_floorCount;
    TrafficPattern m_pattern = TrafficPattern::UpPeak;
    float m_ratePer5Min = 45.0f;
    float m_timeToNext = 0.0f;
    std::mt19937 m_rng;
};

}  // namespace sim
