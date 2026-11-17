// core/traffic_engine.hpp - platform-independent traffic-light state machine.
// Knows: state, direction, durations, remaining time. Does NOT know: INT 10h,
// port addresses, canvases, GPIO. Outputs go through HardwareInterface only.
#pragma once
#include <string>
#include "../hardware/hardware_interface.hpp"
#include "traffic_config.hpp"
#include "traffic_state.hpp"

namespace traffic {

class TrafficEngine {
public:
    explicit TrafficEngine(const TrafficConfig& config = TrafficConfig());

    // Validates the configuration. If invalid, falls back to built-in defaults,
    // stores the reason in *error (if given) and returns false. Always leaves
    // the engine in a usable GREEN state.
    bool initialize(std::string* error = nullptr);

    void setHardware(HardwareInterface* hw);   // may be nullptr (pure logic)

    // Advance time. Negative / non-finite deltas are ignored. Handles deltas
    // that span several phases. Does nothing while paused, in MANUAL mode, or OFF.
    void update(double deltaTimeSeconds);

    // Manual control. Each call validates its input and returns false (state
    // unchanged) on invalid values. setState restarts the phase timer.
    bool setState(LightState s);
    bool setDirection(Direction d);
    bool setDuration(LightState s, double seconds);   // applies to next entry of that phase
    void allOff();
    void reset();                                     // back to GREEN, full time, config direction/mode

    void setPaused(bool p) { paused_ = p; }
    bool isPaused() const  { return paused_; }
    void setMode(Mode m)   { mode_ = m; }
    Mode mode() const      { return mode_; }

    LightState getState() const         { return state_.state; }
    Direction  getDirection() const     { return state_.direction; }
    double     getRemainingTime() const { return state_.remainingTime; }
    const TrafficState&  state() const  { return state_; }
    const TrafficConfig& config() const { return config_; }

private:
    void enter(LightState s);      // set state + full duration, no publish
    void publish();                // push snapshot to hardware
    static LightState next(LightState s);

    TrafficConfig     config_;
    TrafficState      state_;
    HardwareInterface* hw_ = nullptr;
    bool paused_ = false;
    Mode mode_   = Mode::AUTO;
};

}  // namespace traffic
