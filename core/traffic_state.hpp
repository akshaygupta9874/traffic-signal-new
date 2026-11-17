// core/traffic_state.hpp - the snapshot every backend reads.
// Renderers and hardware receive it by const reference: only TrafficEngine
// can modify the authoritative copy.
#pragma once
#include "direction.hpp"

namespace traffic {

enum class LightState : unsigned char {
    GREEN  = 0,
    YELLOW = 1,
    RED    = 2,
    OFF    = 3   // all lamps dark (manual "all off" / shutdown)
};

inline bool isValid(LightState s) { return static_cast<unsigned>(s) <= 3u; }

inline const char* toString(LightState s) {
    switch (s) {
        case LightState::GREEN:  return "GREEN";
        case LightState::YELLOW: return "YELLOW";
        case LightState::RED:    return "RED";
        case LightState::OFF:    return "OFF";
    }
    return "INVALID";
}

struct TrafficState {
    LightState state         = LightState::GREEN;
    Direction  direction     = Direction::STRAIGHT;
    double     remainingTime = 0.0;   // seconds left in this phase
    double     duration      = 0.0;   // full length of this phase, seconds
};

}  // namespace traffic
