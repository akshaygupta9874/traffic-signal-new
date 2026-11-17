// core/traffic_config.hpp - configuration + validation + tiny config-file parser.
#pragma once
#include <iosfwd>
#include <string>
#include "direction.hpp"
#include "traffic_state.hpp"

namespace traffic {

enum class Mode { AUTO, MANUAL };

constexpr double kMinDuration = 1.0;      // seconds
constexpr double kMaxDuration = 3600.0;
constexpr double kMinZoom     = 0.5;
constexpr double kMaxZoom     = 3.0;

struct TrafficConfig {
    double    greenDuration  = 10.0;
    double    yellowDuration = 3.0;
    double    redDuration    = 10.0;
    double    zoom           = 1.0;
    Direction direction      = Direction::STRAIGHT;
    Mode      mode           = Mode::AUTO;

    // Duration of one phase (OFF has none -> 0).
    double durationFor(LightState s) const;

    // Returns an empty string when valid, otherwise a human-readable reason.
    std::string validate() const;
};

// Parses "key = value" lines (# comments). Keys: green, yellow, red, zoom,
// direction, mode. On error returns false and fills `error`; `cfg` is only
// replaced when the whole file is valid.
bool loadConfigFromStream(std::istream& in, TrafficConfig& cfg, std::string& error);
bool loadConfigFile(const std::string& path, TrafficConfig& cfg, std::string& error);

}  // namespace traffic
