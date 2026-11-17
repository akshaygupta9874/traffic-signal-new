#include "traffic_c_api.h"

#include <new>
#include "../../core/traffic_engine.hpp"

using namespace traffic;

namespace {
// Bridges HardwareInterface (what the engine knows) to a C callback (what a phone app provides).
class CallbackHardware : public HardwareInterface {
public:
    const char* name() const override { return "callback"; }
    bool initialize() override { return true; }
    bool isAvailable() const override { return true; }
    void applyState(const TrafficState& s) override {
        if (cb) cb(static_cast<int>(s.state), s.state == LightState::OFF ? 0 : toMask(s.direction), s.remainingTime, user);
    }
    void allOff() override { if (cb) cb(3, 0, 0.0, user); }
    TrafficOutputCallback cb = nullptr;
    void* user = nullptr;
};
}  // namespace

struct TrafficHandle {
    TrafficEngine engine;
    CallbackHardware hw;
    explicit TrafficHandle(const TrafficConfig& c) : engine(c) {}
};

extern "C" {

TrafficHandle* traffic_create(double g, double y, double r) {
    TrafficConfig cfg;
    cfg.greenDuration = g; cfg.yellowDuration = y; cfg.redDuration = r;
    if (!cfg.validate().empty()) return nullptr;
    TrafficHandle* h = new (std::nothrow) TrafficHandle(cfg);
    if (!h) return nullptr;
    h->engine.initialize();
    h->engine.setHardware(&h->hw);
    return h;
}

void traffic_destroy(TrafficHandle* h) { delete h; }

void traffic_set_output_callback(TrafficHandle* h, TrafficOutputCallback cb, void* user) {
    if (!h) return;
    h->hw.cb = cb; h->hw.user = user;
    h->engine.setHardware(&h->hw);                 // republish so the new listener sees the current state
}

void traffic_update(TrafficHandle* h, double dt) { if (h) h->engine.update(dt); }

int traffic_set_state(TrafficHandle* h, int s) {
    return h && s >= 0 && s <= 3 && h->engine.setState(static_cast<LightState>(s)) ? 1 : 0;
}
int traffic_set_direction(TrafficHandle* h, int m) {
    Direction d;
    return h && m >= 0 && directionFromMask(static_cast<unsigned>(m), d) && h->engine.setDirection(d) ? 1 : 0;
}
int traffic_set_duration(TrafficHandle* h, int s, double sec) {
    return h && s >= 0 && s <= 3 && h->engine.setDuration(static_cast<LightState>(s), sec) ? 1 : 0;
}
void traffic_all_off(TrafficHandle* h)  { if (h) h->engine.allOff(); }
void traffic_reset(TrafficHandle* h)    { if (h) h->engine.reset(); }
void traffic_set_paused(TrafficHandle* h, int p) { if (h) h->engine.setPaused(p != 0); }
void traffic_set_manual(TrafficHandle* h, int m) { if (h) h->engine.setMode(m ? Mode::MANUAL : Mode::AUTO); }

int    traffic_get_state(const TrafficHandle* h)     { return h ? static_cast<int>(h->engine.getState()) : 3; }
int    traffic_get_direction(const TrafficHandle* h) { return h ? static_cast<int>(toMask(h->engine.getDirection())) : 0; }
double traffic_get_remaining(const TrafficHandle* h) { return h ? h->engine.getRemainingTime() : 0.0; }
double traffic_get_duration(const TrafficHandle* h)  { return h ? h->engine.state().duration : 0.0; }

}  // extern "C"
