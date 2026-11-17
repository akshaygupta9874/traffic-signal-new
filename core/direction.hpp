// core/direction.hpp - direction model (independent of colour and of any platform).
// Directions are a 3-bit mask so combinations are just bitwise ORs. The same
// numeric values are used by the 8086 engine (DIR_* equates) and by the 8255
// backend (Port B bits), which keeps every layer consistent.
#pragma once
#include <cstdint>
#include <string>

namespace traffic {

enum class Direction : std::uint8_t {
    NONE           = 0,  // plain three-lamp signal, no arrows
    LEFT           = 1,
    STRAIGHT       = 2,
    STRAIGHT_LEFT  = 3,
    RIGHT          = 4,
    LEFT_RIGHT     = 5,
    STRAIGHT_RIGHT = 6,
    ALL            = 7
};

constexpr std::uint8_t kDirLeftBit     = 1;
constexpr std::uint8_t kDirStraightBit = 2;
constexpr std::uint8_t kDirRightBit    = 4;

inline std::uint8_t toMask(Direction d) { return static_cast<std::uint8_t>(d); }
inline bool hasLeft(Direction d)     { return (toMask(d) & kDirLeftBit) != 0; }
inline bool hasStraight(Direction d) { return (toMask(d) & kDirStraightBit) != 0; }
inline bool hasRight(Direction d)    { return (toMask(d) & kDirRightBit) != 0; }

// Validates an integer mask (0..7). Returns false and leaves `out` untouched otherwise.
inline bool directionFromMask(unsigned mask, Direction& out) {
    if (mask > 7u) return false;
    out = static_cast<Direction>(mask);
    return true;
}

inline bool isValid(Direction d) { return toMask(d) <= 7u; }

inline const char* toString(Direction d) {
    switch (d) {
        case Direction::NONE:           return "NONE";
        case Direction::LEFT:           return "LEFT";
        case Direction::STRAIGHT:       return "STRAIGHT";
        case Direction::STRAIGHT_LEFT:  return "STRAIGHT_LEFT";
        case Direction::RIGHT:          return "RIGHT";
        case Direction::LEFT_RIGHT:     return "LEFT_RIGHT";
        case Direction::STRAIGHT_RIGHT: return "STRAIGHT_RIGHT";
        case Direction::ALL:            return "ALL";
    }
    return "INVALID";
}

// Case-insensitive parse of the names above. Returns false for unknown names.
inline bool parseDirection(const std::string& text, Direction& out) {
    std::string t;
    for (char c : text) t += static_cast<char>((c >= 'a' && c <= 'z') ? c - 32 : c);
    for (unsigned m = 0; m <= 7; ++m) {
        Direction d = static_cast<Direction>(m);
        if (t == toString(d)) { out = d; return true; }
    }
    return false;
}

// Next direction in the 0..7 cycle (used by the "C" key).
inline Direction nextDirection(Direction d) {
    return static_cast<Direction>((toMask(d) + 1u) & 7u);
}

}  // namespace traffic
