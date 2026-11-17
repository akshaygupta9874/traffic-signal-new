// hardware/hardware_interface.hpp - what the core is allowed to know about outputs.
// No port numbers, no DOS, no GPIO here. Backends translate TrafficState.
#pragma once
#include "../core/traffic_state.hpp"

namespace traffic {

class HardwareInterface {
public:
    virtual ~HardwareInterface() = default;
    virtual const char* name() const = 0;
    // Prepare the device. Returns false if it cannot be used.
    virtual bool initialize() = 0;
    virtual bool isAvailable() const = 0;
    // Drive lamps/arrows to match the snapshot.
    virtual void applyState(const TrafficState& state) = 0;
    virtual void allOff() = 0;
    virtual void shutdown() { allOff(); }
};

}  // namespace traffic
