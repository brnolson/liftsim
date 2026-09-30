#pragma once

namespace sim {

enum class DoorState { Closed, Opening, Open, Closing };

// Center-opening car door operator. The car door drives the landing door at
// the same floor through a mechanical coupler, so one operator animates both.
// A light curtain across the entrance reopens the doors if the beam is broken
// while closing.
class DoorOperator {
public:
    DoorOperator(float openTime, float closeTime) : m_openTime(openTime), m_closeTime(closeTime) {}

    void Open();
    void Close();
    void Update(float dt, bool beamBlocked);

    DoorState State() const { return m_state; }
    bool  IsClosed() const { return m_state == DoorState::Closed; }
    bool  IsFullyOpen() const { return m_state == DoorState::Open; }
    float OpenFraction() const { return m_fraction; }   // 0 = closed, 1 = open
    // Eased panel travel: door operators accelerate and brake the panels.
    float PanelTravel() const;
    int   ReopenCount() const { return m_reopenCount; }

private:
    DoorState m_state = DoorState::Closed;
    float m_fraction = 0.0f;
    float m_openTime, m_closeTime;
    int   m_reopenCount = 0;
};

}  // namespace sim
