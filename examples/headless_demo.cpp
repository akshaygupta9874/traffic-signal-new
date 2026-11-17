// Runs the engine against the 8255 backend (in-memory chip) and prints the port
// writes - the same bytes the DOS program sends with OUT. Fast-forwards time.
//   ./build/headless_demo [config-file]
#include <iomanip>
#include <iostream>
#include "../core/traffic_engine.hpp"
#include "../hardware/hardware_select.hpp"
#include "../hardware/i8255_backend.hpp"
#include "../hardware/simulated_hardware.hpp"

using namespace traffic;

int main(int argc, char** argv) {
    TrafficConfig cfg;
    std::string err;
    if (argc > 1 && !loadConfigFile(argv[1], cfg, err)) {
        std::cerr << "config error: " << err << " - using defaults\n";
        cfg = TrafficConfig();
    }
    Simulated8255 chip;
    I8255Backend ppi(chip);
    SimulatedHardware sim;
    std::string note;
    HardwareInterface* hw = selectHardware(&ppi, sim, &note);
    if (!note.empty()) std::cout << note << "\n";

    TrafficEngine eng(cfg);
    if (!eng.initialize(&err)) std::cerr << "config invalid (" << err << "), defaults used\n";
    eng.setHardware(hw);

    std::cout << "backend: " << hw->name() << "\n t(s)  state   dir             left  PortA PortB\n";
    LightState last = LightState::OFF;
    for (int t = 0; t <= 40; ++t) {
        if (eng.getState() != last) {
            last = eng.getState();
            std::cout << std::setw(4) << t << "  " << std::left << std::setw(7) << toString(last) << std::setw(16)
                      << toString(eng.getDirection()) << std::right << std::setw(4) << eng.getRemainingTime()
                      << "   0x" << std::hex << std::setw(2) << std::setfill('0') << int(chip.portA) << "  0x"
                      << std::setw(2) << int(chip.portB) << std::dec << std::setfill(' ') << "\n";
        }
        eng.update(1.0);
    }
    eng.allOff();
    hw->shutdown();
    return 0;
}
