#include "sim/DoorOperator.h"
#include <algorithm>

namespace sim {

void DoorOperator::Open() {
    if (m_state != DoorState::Open) m_state = DoorState::Opening;
}

void DoorOperator::Close() {
    if (m_state != DoorState::Closed) m_state = DoorState::Closing;
}

void DoorOperator::Update(float dt, bool beamBlocked) {
    switch (m_state) {
    case DoorState::Opening:
        m_fraction = std::min(1.0f, m_fraction + dt / m_openTime);
        if (m_fraction >= 1.0f) m_state = DoorState::Open;
        break;

    case DoorState::Closing:
        if (beamBlocked) {              // light curtain: reverse immediately
            m_state = DoorState::Opening;
            ++m_reopenCount;
            break;
        }
        m_fraction = std::max(0.0f, m_fraction - dt / m_closeTime);
        if (m_fraction <= 0.0f) m_state = DoorState::Closed;
        break;

    case DoorState::Open:
    case DoorState::Closed:
        break;
    }
}

float DoorOperator::PanelTravel() const {
    // Smoothstep: zero velocity at both ends of the stroke.
    float t = m_fraction;
    return t * t * (3.0f - 2.0f * t);
}

}  // namespace sim
