#include "i8255_backend.hpp"

namespace traffic {

using namespace i8255;

std::uint8_t I8255Backend::portAValue(LightState s) {
    switch (s) {
        case LightState::GREEN:  return kBitGreen;
        case LightState::YELLOW: return kBitYellow;
        case LightState::RED:    return kBitRed;
        default:                 return 0x00;
    }
}

std::uint8_t I8255Backend::portBValue(const TrafficState& s) {
    return s.state == LightState::OFF ? 0x00 : toMask(s.direction);
}

bool I8255Backend::initialize() {
    available_ = false;
    if (!io_.isOpen()) return false;
    io_.write(kControl, kControlWordAllOutputMode0);
    // Probe: in mode 0 a real chip returns the output latch on read-back.
    io_.write(kPortA, 0xA5);
    available_ = (io_.read(kPortA) == 0xA5);
    io_.write(kPortA, 0x00);
    io_.write(kPortB, 0x00);
    return available_;
}

void I8255Backend::applyState(const TrafficState& s) {
    if (!available_) return;
    io_.write(kPortA, portAValue(s.state));
    io_.write(kPortB, portBValue(s));
}

void I8255Backend::allOff() {
    if (!available_) return;
    io_.write(kPortA, 0x00);
    io_.write(kPortB, 0x00);
}

void Simulated8255::write(std::uint16_t port, std::uint8_t value) {
    ++writeCount;
    if (!present_) return;
    if (port == kPortA) portA = value;
    else if (port == kPortB) portB = value;
    else if (port == kPortC) portC = value;
    else if (port == kControl) control = value;
}

std::uint8_t Simulated8255::read(std::uint16_t port) {
    if (!present_) return 0xFF;
    if (port == kPortA) return portA;
    if (port == kPortB) return portB;
    if (port == kPortC) return portC;
    return 0xFF;
}

}  // namespace traffic
