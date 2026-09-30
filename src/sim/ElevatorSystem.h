#pragma once

#include "sim/Car.h"
#include "sim/Traffic.h"
#include <array>
#include <random>
#include <vector>

namespace sim {

enum class PassengerState { Waiting, Riding, Arrived };

struct Passenger {
    int   id = 0;
    int   origin = 0;
    int   destination = 0;
    float massKg = kPassengerMassKg;
    float spawnTime = 0.0f;
    float boardTime = -1.0f;
    float arriveTime = -1.0f;
    int   car = -1;
    PassengerState state = PassengerState::Waiting;

    Direction Dir() const { return DirectionBetween(origin, destination); }
};

// A lit hall button. The group controller assigns each call to one car.
struct HallCall {
    bool  active = false;
    int   assignedCar = -1;
    float registeredAt = 0.0f;
};

// Key performance indicators used to judge an elevator installation.
struct Stats {
    int    served = 0;
    double totalWait = 0.0;      // hall call to boarding
    double totalJourney = 0.0;   // arrival at landing to arrival at destination
    float  maxWait = 0.0f;
    int    longWaits = 0;        // waits over 60 s

    float AvgWait() const { return served ? float(totalWait / served) : 0.0f; }
    float AvgJourney() const { return served ? float(totalJourney / served) : 0.0f; }
    float PercentLongWaits() const { return served ? 100.0f * longWaits / served : 0.0f; }
};

// The group controller: owns every car, registers hall calls, assigns each
// call to the car with the lowest estimated time of arrival, and runs each
// car's stop/door/boarding sequence.
class ElevatorSystem {
public:
    ElevatorSystem(const BuildingSpec& building, const ElevatorSpec& spec, unsigned seed = 42);

    void Update(float dt);

    int  AddPassenger(int origin, int destination);
    void InjectDriveFault(int car);
    void ResetCar(int car);

    TrafficGenerator& Traffic() { return m_traffic; }
    const TrafficGenerator& Traffic() const { return m_traffic; }
    const BuildingSpec& Building() const { return m_building; }
    const ElevatorSpec& Spec() const { return m_spec; }
    const std::vector<Car>& Cars() const { return m_cars; }
    const std::vector<Passenger>& Passengers() const { return m_passengers; }
    const std::vector<int>& WaitingAt(int floor) const { return m_waiting[floor]; }
    const HallCall& GetHallCall(int floor, Direction d) const { return m_hallCalls[floor][Slot(d)]; }
    const Stats& GetStats() const { return m_stats; }
    float Time() const { return m_time; }

    // Estimated seconds until `car` could answer a hall call (public for tests/HUD).
    float EstimateArrivalTime(const Car& car, int floor, Direction d) const;

private:
    static int Slot(Direction d) { return d == Direction::Up ? 0 : 1; }
    HallCall& Call(int floor, Direction d) { return m_hallCalls[floor][Slot(d)]; }
    bool IsAssigned(int floor, Direction d, const Car& car) const;

    // Hall calls and dispatching
    void RegisterHallCall(int floor, Direction d);
    void ClearHallCall(int floor, Direction d);
    int  BestCarFor(int floor, Direction d) const;
    void ReallocateHallCalls();

    // Per-car sequencing
    void UpdateCarLogic(Car& car, float dt);
    int  ChooseNextStop(const Car& car, int fromFloor) const;
    int  NextStopInDirection(const Car& car, int fromFloor, Direction d) const;
    bool HasStopsBeyond(const Car& car, int floor, Direction d) const;
    int  CountStopsBetween(const Car& car, int a, int b) const;
    Direction DirectionAfterStop(const Car& car, int floor) const;
    void ArriveAtFloor(Car& car);
    void TransferPassengers(Car& car, float dt);
    void ParkIfIdle(Car& car);

    BuildingSpec m_building;
    ElevatorSpec m_spec;
    std::vector<Car> m_cars;
    std::vector<Passenger> m_passengers;
    std::vector<std::vector<int>> m_waiting;               // passenger ids per floor, FIFO
    std::vector<std::array<HallCall, 2>> m_hallCalls;      // [floor][up, down]
    TrafficGenerator m_traffic;
    std::vector<TripRequest> m_arrivals;
    Stats m_stats;
    std::mt19937 m_rng;
    float m_time = 0.0f;
    float m_reallocTimer = 0.0f;
};

}  // namespace sim
