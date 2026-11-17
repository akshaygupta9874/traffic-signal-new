#include "traffic_engine.hpp"

#include <cmath>

namespace traffic {

namespace {
constexpr double kEps = 1e-9;
}

TrafficEngine::TrafficEngine(const TrafficConfig& config) : config_(config) {
    mode_ = config_.mode;
    state_.direction = config_.direction;
    enter(LightState::GREEN);
}

bool TrafficEngine::initialize(std::string* error) {
    std::string why = config_.validate();
    bool ok = why.empty();
    if (!ok) {
        if (error) *error = why;
        config_ = TrafficConfig();          // safe defaults
    }
    mode_ = config_.mode;
    paused_ = false;
    state_.direction = config_.direction;
    enter(LightState::GREEN);
    publish();
    return ok;
}

void TrafficEngine::setHardware(HardwareInterface* hw) {
    hw_ = hw;
    publish();
}

LightState TrafficEngine::next(LightState s) {
    switch (s) {
        case LightState::GREEN:  return LightState::YELLOW;
        case LightState::YELLOW: return LightState::RED;
        case LightState::RED:    return LightState::GREEN;
        default:                 return LightState::GREEN;
    }
}

void TrafficEngine::enter(LightState s) {
    state_.state = s;
    state_.duration = config_.durationFor(s);
    state_.remainingTime = state_.duration;
}

void TrafficEngine::publish() {
    if (hw_) hw_->applyState(state_);
}

void TrafficEngine::update(double dt) {
    if (!std::isfinite(dt) || dt <= 0.0) return;
    if (paused_ || mode_ != Mode::AUTO || state_.state == LightState::OFF) return;

    // Skip whole cycles so absurd deltas stay O(1).
    double cycle = config_.greenDuration + config_.yellowDuration + config_.redDuration;
    if (dt > cycle) dt -= std::floor(dt / cycle) * cycle;

    while (dt > kEps) {
        if (dt < state_.remainingTime - kEps) {
            state_.remainingTime -= dt;
            break;
        }
        dt -= state_.remainingTime;
        enter(next(state_.state));
    }
    publish();   // every live update doubles as a heartbeat for serial/fail-safe backends
}

bool TrafficEngine::setState(LightState s) {
    if (!traffic::isValid(s)) return false;
    if (s == LightState::OFF) { allOff(); return true; }
    enter(s);
    publish();
    return true;
}

bool TrafficEngine::setDirection(Direction d) {
    if (!traffic::isValid(d)) return false;
    state_.direction = d;
    publish();
    return true;
}

bool TrafficEngine::setDuration(LightState s, double seconds) {
    if (!std::isfinite(seconds) || seconds < kMinDuration || seconds > kMaxDuration) return false;
    switch (s) {
        case LightState::GREEN:  config_.greenDuration = seconds;  break;
        case LightState::YELLOW: config_.yellowDuration = seconds; break;
        case LightState::RED:    config_.redDuration = seconds;    break;
        default: return false;
    }
    return true;
}

void TrafficEngine::allOff() {
    state_.state = LightState::OFF;
    state_.duration = 0.0;
    state_.remainingTime = 0.0;
    if (hw_) hw_->allOff();
}

void TrafficEngine::reset() {
    paused_ = false;
    mode_ = config_.mode;
    state_.direction = config_.direction;
    enter(LightState::GREEN);
    publish();
}

}  // namespace traffic
