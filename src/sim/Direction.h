#pragma once

namespace sim {

enum class Direction { None, Up, Down };

inline Direction Opposite(Direction d) {
    if (d == Direction::Up) return Direction::Down;
    if (d == Direction::Down) return Direction::Up;
    return Direction::None;
}

// +1 for up, -1 for down, 0 for none; used for floor arithmetic.
inline int Step(Direction d) {
    return d == Direction::Up ? 1 : (d == Direction::Down ? -1 : 0);
}

inline Direction DirectionBetween(int fromFloor, int toFloor) {
    if (toFloor > fromFloor) return Direction::Up;
    if (toFloor < fromFloor) return Direction::Down;
    return Direction::None;
}

inline const char* ToString(Direction d) {
    return d == Direction::Up ? "UP" : (d == Direction::Down ? "DN" : "--");
}

}  // namespace sim
