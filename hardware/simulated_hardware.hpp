// hardware/simulated_hardware.hpp - "virtual LEDs". Used for tests, headless runs,
// and as the automatic fallback when real hardware is unavailable.
#pragma once
#include "hardware_interface.hpp"

namespace traffic {

class SimulatedHardware : public HardwareInterface {
public:
    const char* name() const override { return "simulated"; }
    bool initialize() override { ready_ = true; return true; }
    bool isAvailable() const override { return ready_; }
    void applyState(const TrafficState& s) override {
        last_ = s;
        ++applyCount_;
        if (s.state == LightState::OFF) { clearLamps(); return; }
        green_  = s.state == LightState::GREEN;
        yellow_ = s.state == LightState::YELLOW;
        red_    = s.state == LightState::RED;
        directionMask_ = toMask(s.direction);
    }
    void allOff() override { clearLamps(); last_.state = LightState::OFF; ++applyCount_; }

    bool red() const { return red_; }
    bool yellow() const { return yellow_; }
    bool green() const { return green_; }
    unsigned directionMask() const { return directionMask_; }
    int applyCount() const { return applyCount_; }
    const TrafficState& last() const { return last_; }

private:
    void clearLamps() { red_ = yellow_ = green_ = false; directionMask_ = 0; }
    bool ready_ = false, red_ = false, yellow_ = false, green_ = false;
    unsigned directionMask_ = 0;
    int applyCount_ = 0;
    TrafficState last_;
};

}  // namespace traffic
