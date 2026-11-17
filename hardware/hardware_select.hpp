// hardware/hardware_select.hpp - "use the real thing, else fall back to simulation".
#pragma once
#include <string>
#include "hardware_interface.hpp"

namespace traffic {

// Returns `preferred` if it initialises and reports available; otherwise
// initialises and returns `fallback` and describes why in *note.
inline HardwareInterface* selectHardware(HardwareInterface* preferred,
                                         HardwareInterface& fallback,
                                         std::string* note = nullptr) {
    if (preferred && preferred->initialize() && preferred->isAvailable()) return preferred;
    if (note) {
        *note = std::string(preferred ? preferred->name() : "(none)") +
                " unavailable; using " + fallback.name();
    }
    fallback.initialize();
    return &fallback;
}

}  // namespace traffic
