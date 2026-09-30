#include "sim/ElevatorSystem.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace sim {

static constexpr float kReallocPeriod = 1.0f;     // s between dispatch reviews
static constexpr float kReallocMargin = 5.0f;     // s improvement needed to switch cars
static constexpr float kFullCarPenalty = 60.0f;   // s added to a car that will bypass calls
static constexpr float kDwellAfterTransfer = 1.5f;

ElevatorSystem::ElevatorSystem(const BuildingSpec& building, const ElevatorSpec& spec, unsigned seed)
    : m_building(building), m_spec(spec),
      m_waiting(building.floorCount),
      m_hallCalls(building.floorCount),
      m_traffic(building.floorCount, seed),
      m_rng(seed + 1) {
    for (int i = 0; i < building.carCount; ++i) m_cars.emplace_back(i, spec, building);
}

void ElevatorSystem::Update(float dt) {
    m_time += dt;

    m_arrivals.clear();
    m_traffic.Update(dt, m_arrivals);
    for (const TripRequest& trip : m_arrivals) AddPassenger(trip.origin, trip.destination);

    for (Car& car : m_cars) car.Update(dt);          // physics
    for (Car& car : m_cars) UpdateCarLogic(car, dt); // decisions

    m_reallocTimer -= dt;
    if (m_reallocTimer <= 0.0f) {
        ReallocateHallCalls();
        m_reallocTimer = kReallocPeriod;
    }
}

// ===========================================================================
// Passengers and hall calls
// ===========================================================================
int ElevatorSystem::AddPassenger(int origin, int destination) {
    Passenger p;
    p.id = static_cast<int>(m_passengers.size());
    p.origin = origin;
    p.destination = destination;
    p.massKg = std::uniform_real_distribution<float>(60.0f, 90.0f)(m_rng);
    p.spawnTime = m_time;
    m_passengers.push_back(p);
    m_waiting[origin].push_back(p.id);
    RegisterHallCall(origin, p.Dir());
    return p.id;
}

bool ElevatorSystem::IsAssigned(int floor, Direction d, const Car& car) const {
    const HallCall& call = GetHallCall(floor, d);
    return call.active && call.assignedCar == car.Id();
}

void ElevatorSystem::RegisterHallCall(int floor, Direction d) {
    // A car still closing its doors here in the right direction simply reopens.
    for (Car& car : m_cars) {
        bool sameDirection = car.direction == d || car.direction == Direction::None;
        if (car.mode == CarMode::DoorsClosing && car.NearestFloor() == floor &&
            sameDirection && car.LoadFraction() < m_spec.fullLoadBypass) {
            car.OpenDoors();
            return;
        }
    }

    HallCall& call = Call(floor, d);
    if (call.active) return;
    call.active = true;
    call.registeredAt = m_time;
    call.assignedCar = BestCarFor(floor, d);
}

void ElevatorSystem::ClearHallCall(int floor, Direction d) {
    HallCall& call = Call(floor, d);
    call.active = false;
    call.assignedCar = -1;
}

int ElevatorSystem::BestCarFor(int floor, Direction d) const {
    int best = -1;
    float bestTime = std::numeric_limits<float>::infinity();
    for (const Car& car : m_cars) {
        float eta = EstimateArrivalTime(car, floor, d);
        if (eta < bestTime) { bestTime = eta; best = car.Id(); }
    }
    return best;
}

// Estimated time of arrival (ETA) dispatching: simulate, roughly, the route
// the car will take under collective control and add up travel and stop times.
float ElevatorSystem::EstimateArrivalTime(const Car& car, int floor, Direction d) const {
    if (car.mode == CarMode::OutOfService || car.HasDriveFault())
        return std::numeric_limits<float>::infinity();

    const float v = m_spec.ratedSpeed, a = m_spec.maxAccel, j = m_spec.maxJerk;
    const float h = m_building.floorHeight;
    const float runOverhead = v / a + a / j;   // extra time per start/stop vs. cruising
    const float stopTime = m_spec.doorOpenTime + m_spec.doorCloseTime +
                           2.0f * m_spec.transferTime + kDwellAfterTransfer;

    float t = 0.0f;
    if (car.mode == CarMode::DoorsOpening || car.mode == CarMode::DoorsOpen)
        t += std::max(car.dwellTimer, 0.0f) + m_spec.doorCloseTime;
    else if (car.mode == CarMode::DoorsClosing)
        t += m_spec.doorCloseTime * car.Doors().OpenFraction();

    int from = car.CommittedFloor();
    Direction cd = car.direction;
    bool hasWork = HasStopsBeyond(car, from, Direction::Up) || HasStopsBeyond(car, from, Direction::Down);

    float distanceFloors;
    int stops;
    if (cd == Direction::None || !hasWork) {
        distanceFloors = static_cast<float>(std::abs(floor - from));
        stops = 0;
    } else {
        bool ahead = Step(cd) * (floor - from) >= 0;
        if (ahead && (d == cd || !HasStopsBeyond(car, floor, cd))) {
            distanceFloors = static_cast<float>(std::abs(floor - from));
            stops = CountStopsBetween(car, from, floor);
        } else {
            // Car finishes its current sweep, turns around, comes back.
            int turn = from;
            for (int f = from; f >= 0 && f < m_building.floorCount; f += Step(cd))
                if (car.carCalls[f] || IsAssigned(f, Direction::Up, car) || IsAssigned(f, Direction::Down, car))
                    turn = f;
            distanceFloors = static_cast<float>(std::abs(turn - from) + std::abs(turn - floor));
            stops = CountStopsBetween(car, from, turn) + CountStopsBetween(car, turn, floor) + 1;
        }
    }

    if (distanceFloors > 0.0f) t += distanceFloors * h / v + (stops + 1) * runOverhead;
    t += stops * stopTime;
    if (car.LoadFraction() >= m_spec.fullLoadBypass) t += kFullCarPenalty;
    return t;
}

void ElevatorSystem::ReallocateHallCalls() {
    for (int f = 0; f < m_building.floorCount; ++f) {
        for (Direction d : {Direction::Up, Direction::Down}) {
            HallCall& call = Call(f, d);
            if (!call.active) continue;

            // Never take a call away from a car that is already slowing down for it.
            if (call.assignedCar >= 0) {
                const Car& current = m_cars[call.assignedCar];
                if (current.IsRunning() && current.TargetFloor() == f && current.direction == d)
                    continue;
            }

            int best = BestCarFor(f, d);
            if (best < 0 || best == call.assignedCar) continue;

            float currentEta = call.assignedCar >= 0
                ? EstimateArrivalTime(m_cars[call.assignedCar], f, d)
                : std::numeric_limits<float>::infinity();
            if (EstimateArrivalTime(m_cars[best], f, d) + kReallocMargin < currentEta)
                call.assignedCar = best;
        }
    }
}

// ===========================================================================
// Per-car sequencing (selective collective control)
// ===========================================================================
void ElevatorSystem::UpdateCarLogic(Car& car, float dt) {
    switch (car.mode) {
    case CarMode::Idle: {
        int here = car.NearestFloor();
        int next = ChooseNextStop(car, here);
        if (next < 0) { ParkIfIdle(car); break; }
        if (next == here) { ArriveAtFloor(car); break; }
        car.direction = DirectionBetween(here, next);
        car.StartRun(next);
        break;
    }

    case CarMode::Running: {
        if (car.HasDriveFault()) break;               // no longer under control
        if (!car.IsRunning()) { ArriveAtFloor(car); break; }

        // Take a newly registered stop ahead if the car can still brake for it.
        int committed = car.CommittedFloor();
        int next = ChooseNextStop(car, committed);
        int target = car.TargetFloor();
        int step = Step(DirectionBetween(car.NearestFloor(), target));
        bool between = step != 0 && (next - committed) * step >= 0 && (target - next) * step > 0;
        if (next >= 0 && between && car.CanStopAt(next)) car.Retarget(next);
        break;
    }

    case CarMode::DoorsOpening:
        if (car.Doors().IsFullyOpen()) {
            car.mode = CarMode::DoorsOpen;
            car.dwellTimer = m_spec.doorDwellTime;
            car.transferTimer = 0.0f;
        }
        break;

    case CarMode::DoorsOpen:
        TransferPassengers(car, dt);
        break;

    case CarMode::DoorsClosing:
        car.beamBlocked = false;
        if (car.Doors().State() == DoorState::Opening) {
            car.mode = CarMode::DoorsOpening;         // light curtain reopened them
        } else if (car.Doors().IsClosed()) {
            car.mode = CarMode::Idle;
            // Anyone left behind (car was full) presses the button again.
            int f = car.NearestFloor();
            for (int id : m_waiting[f]) RegisterHallCall(f, m_passengers[id].Dir());
        }
        break;

    case CarMode::OutOfService:
        car.beamBlocked = false;
        break;
    }
}

int ElevatorSystem::ChooseNextStop(const Car& car, int fromFloor) const {
    if (car.direction == Direction::None) {
        // Idle car: go to the nearest floor with any work.
        int best = -1, bestDistance = m_building.floorCount + 1;
        for (int f = 0; f < m_building.floorCount; ++f) {
            bool work = car.carCalls[f] || IsAssigned(f, Direction::Up, car) || IsAssigned(f, Direction::Down, car);
            if (work && std::abs(f - fromFloor) < bestDistance) { best = f; bestDistance = std::abs(f - fromFloor); }
        }
        return best;
    }
    int next = NextStopInDirection(car, fromFloor, car.direction);
    if (next >= 0) return next;
    return NextStopInDirection(car, fromFloor, Opposite(car.direction));
}

// Collective control, one sweep: first any car call or same-direction hall
// call ahead of us; failing that, the farthest opposite-direction hall call
// (the car turns around there, like the "elevator algorithm" / SCAN).
int ElevatorSystem::NextStopInDirection(const Car& car, int fromFloor, Direction d) const {
    const int step = Step(d);
    const int last = (d == Direction::Up) ? m_building.floorCount - 1 : 0;
    const bool bypassHallCalls = car.LoadFraction() >= m_spec.fullLoadBypass;

    for (int f = fromFloor; f >= 0 && f < m_building.floorCount; f += step) {
        if (car.carCalls[f]) return f;
        if (!bypassHallCalls && IsAssigned(f, d, car)) return f;
    }
    if (bypassHallCalls) return -1;
    for (int f = last; f != fromFloor - step; f -= step) {
        if (IsAssigned(f, Opposite(d), car)) return f;
    }
    return -1;
}

bool ElevatorSystem::HasStopsBeyond(const Car& car, int floor, Direction d) const {
    for (int f = floor + Step(d); f >= 0 && f < m_building.floorCount; f += Step(d)) {
        if (car.carCalls[f] || IsAssigned(f, Direction::Up, car) || IsAssigned(f, Direction::Down, car))
            return true;
    }
    return false;
}

int ElevatorSystem::CountStopsBetween(const Car& car, int a, int b) const {
    int count = 0;
    for (int f = std::min(a, b) + 1; f < std::max(a, b); ++f) {
        if (car.carCalls[f] || IsAssigned(f, Direction::Up, car) || IsAssigned(f, Direction::Down, car))
            ++count;
    }
    return count;
}

// Which way will the hall lantern point when the doors open here?
Direction ElevatorSystem::DirectionAfterStop(const Car& car, int floor) const {
    bool upHere = GetHallCall(floor, Direction::Up).active;
    bool downHere = GetHallCall(floor, Direction::Down).active;
    Direction d = car.direction == Direction::None ? Direction::Up : car.direction;

    // Keep going the same way if someone here or beyond needs that direction.
    bool hereSame = (d == Direction::Up) ? upHere : downHere;
    bool hereOpposite = (d == Direction::Up) ? downHere : upHere;
    if (hereSame || HasStopsBeyond(car, floor, d)) return d;
    if (hereOpposite || HasStopsBeyond(car, floor, Opposite(d))) return Opposite(d);
    return Direction::None;
}

void ElevatorSystem::ArriveAtFloor(Car& car) {
    int f = car.NearestFloor();
    bool carCallHere = car.carCalls[f];
    car.carCalls[f] = false;
    car.direction = DirectionAfterStop(car, f);

    bool hallCallHere = car.direction != Direction::None && GetHallCall(f, car.direction).active;
    if (hallCallHere) ClearHallCall(f, car.direction);

    if (!carCallHere && !hallCallHere) {
        car.mode = CarMode::Idle;    // e.g. parking run finished
        return;
    }
    car.OpenDoors();
}

void ElevatorSystem::TransferPassengers(Car& car, float dt) {
    int f = car.NearestFloor();

    // Someone is still walking through the doorway.
    car.transferTimer -= dt;
    if (car.transferTimer > 0.0f) { car.beamBlocked = true; return; }
    car.beamBlocked = false;

    // 1) Let passengers out first.
    for (size_t i = 0; i < car.riders.size(); ++i) {
        Passenger& p = m_passengers[car.riders[i]];
        if (p.destination != f) continue;
        p.state = PassengerState::Arrived;
        p.arriveTime = m_time;
        car.AddLoad(-p.massKg);
        car.riders.erase(car.riders.begin() + static_cast<long>(i));

        float wait = p.boardTime - p.spawnTime;
        m_stats.served++;
        m_stats.totalWait += wait;
        m_stats.totalJourney += p.arriveTime - p.spawnTime;
        m_stats.maxWait = std::max(m_stats.maxWait, wait);
        if (wait > 60.0f) m_stats.longWaits++;

        car.transferTimer = m_spec.transferTime;
        car.dwellTimer = kDwellAfterTransfer;
        return;
    }

    // 2) Then let waiting passengers in, if they travel our way and fit.
    std::vector<int>& queue = m_waiting[f];
    for (size_t i = 0; i < queue.size(); ++i) {
        Passenger& p = m_passengers[queue[i]];
        bool ourWay = car.direction == Direction::None || p.Dir() == car.direction;
        if (!ourWay || !car.HasRoomFor(p.massKg)) continue;

        if (car.direction == Direction::None) {
            car.direction = p.Dir();
            ClearHallCall(f, car.direction);
        }
        p.state = PassengerState::Riding;
        p.boardTime = m_time;
        p.car = car.Id();
        car.riders.push_back(p.id);
        car.AddLoad(p.massKg);
        car.carCalls[p.destination] = true;
        queue.erase(queue.begin() + static_cast<long>(i));

        // Last person going this way got in: turn off the hall button.
        bool othersSameWay = std::any_of(queue.begin(), queue.end(),
            [&](int id) { return m_passengers[id].Dir() == p.Dir(); });
        if (!othersSameWay) ClearHallCall(f, p.Dir());

        car.transferTimer = m_spec.transferTime;
        car.dwellTimer = kDwellAfterTransfer;
        return;
    }

    // 3) Nobody moving: hold the doors for the dwell time, then close.
    car.dwellTimer -= dt;
    if (car.dwellTimer <= 0.0f) car.CloseDoors();
}

void ElevatorSystem::ParkIfIdle(Car& car) {
    // During up-peak, empty cars return to the lobby where demand is.
    car.direction = Direction::None;
    if (m_traffic.Pattern() == TrafficPattern::UpPeak && car.NearestFloor() != 0) {
        car.direction = Direction::Down;
        car.StartRun(0);
    }
}

// ===========================================================================
// Fault injection
// ===========================================================================
void ElevatorSystem::InjectDriveFault(int car) {
    if (car >= 0 && car < static_cast<int>(m_cars.size())) m_cars[car].InjectDriveFault();
}

void ElevatorSystem::ResetCar(int car) {
    if (car < 0 || car >= static_cast<int>(m_cars.size())) return;
    Car& c = m_cars[car];
    if (c.mode != CarMode::OutOfService) return;
    c.ResetFaults();
    c.direction = Direction::None;
    if (!c.Doors().IsClosed()) c.CloseDoors();
}

}  // namespace sim
