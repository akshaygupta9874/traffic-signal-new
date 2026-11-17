// hardware/i8255_backend.hpp - 8255 PPI driver. ALL port numbers live here.
//
// Mapping (same as the original DOS program):
//   Port A 80H : PA0 = GREEN, PA1 = YELLOW, PA2 = RED
//   Port B 82H : PB0 = LEFT, PB1 = STRAIGHT, PB2 = RIGHT arrow   (NEW, optional wiring)
//   Port C 84H : unused
//   Control 86H: 80H = mode 0, A/B/C all outputs
//
// The driver talks to a PortIO object, so the same code runs against a real
// port-I/O implementation, or against the in-memory Simulated8255 used in tests.
#pragma once
#include <cstdint>
#include "hardware_interface.hpp"

namespace traffic {

class PortIO {
public:
    virtual ~PortIO() = default;
    virtual bool isOpen() const = 0;
    virtual void write(std::uint16_t port, std::uint8_t value) = 0;
    virtual std::uint8_t read(std::uint16_t port) = 0;
};

namespace i8255 {
constexpr std::uint16_t kPortA   = 0x80;
constexpr std::uint16_t kPortB   = 0x82;
constexpr std::uint16_t kPortC   = 0x84;
constexpr std::uint16_t kControl = 0x86;
constexpr std::uint8_t  kControlWordAllOutputMode0 = 0x80;
constexpr std::uint8_t  kBitGreen  = 0x01;   // PA0
constexpr std::uint8_t  kBitYellow = 0x02;   // PA1
constexpr std::uint8_t  kBitRed    = 0x04;   // PA2
}  // namespace i8255

class I8255Backend : public HardwareInterface {
public:
    explicit I8255Backend(PortIO& io) : io_(io) {}
    const char* name() const override { return "8255"; }
    bool initialize() override;
    bool isAvailable() const override { return available_; }
    void applyState(const TrafficState& state) override;
    void allOff() override;

    // Pure translation, exposed for tests and documentation.
    static std::uint8_t portAValue(LightState s);
    static std::uint8_t portBValue(const TrafficState& s);

private:
    PortIO& io_;
    bool available_ = false;
};

// In-memory 8255 model: latches written values and records the write log.
// Set present=false to emulate a missing chip (reads return 0xFF, like a floating bus).
class Simulated8255 : public PortIO {
public:
    explicit Simulated8255(bool present = true, bool open = true) : present_(present), open_(open) {}
    bool isOpen() const override { return open_; }
    void write(std::uint16_t port, std::uint8_t value) override;
    std::uint8_t read(std::uint16_t port) override;
    std::uint8_t portA = 0, portB = 0, portC = 0, control = 0;
    int writeCount = 0;
private:
    bool present_, open_;
};

}  // namespace traffic
