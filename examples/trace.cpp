// Prints a deterministic trace of the engine. tests/cross_check.sh compares this
// byte-for-byte with the JavaScript engine to prove both implement the same machine.
//   ./build/trace <green> <yellow> <red> <dt>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "../core/traffic_engine.hpp"

using namespace traffic;

int main(int argc, char** argv) {
    TrafficConfig cfg;
    if (argc >= 5) {
        cfg.greenDuration = std::atof(argv[1]);
        cfg.yellowDuration = std::atof(argv[2]);
        cfg.redDuration = std::atof(argv[3]);
    }
    const double dt = argc >= 5 ? std::atof(argv[4]) : 0.25;
    TrafficEngine e(cfg);
    e.initialize();
    for (int i = 0; i <= 400; ++i) {
        std::printf("%d %s %s %d\n", i, toString(e.getState()), toString(e.getDirection()),
                    static_cast<int>(std::ceil(e.getRemainingTime() - 1e-9)));
        if (i == 100) e.setState(LightState::RED);          // manual jump mid-run
        if (i == 150) e.setDirection(Direction::LEFT_RIGHT);
        if (i == 200) e.setPaused(true);
        if (i == 210) e.setPaused(false);
        if (i == 300) e.allOff();
        if (i == 320) e.reset();
        e.update(dt);
    }
    return 0;
}
