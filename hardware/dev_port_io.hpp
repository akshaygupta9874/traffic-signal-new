// hardware/dev_port_io.hpp - raw x86 port I/O through /dev/port (Linux only).
//
// WARNING: on an ordinary PC, ports 80H-86H are NOT an 8255. 80H is the POST
// diagnostic port and 81H-8FH are DMA page registers. Writing there on a real
// PC can disturb DMA. Use this ONLY with an 8255 board you have placed at
// these addresses (trainer kit / ISA card) and where you know it is safe.
// Needs root. If the device cannot be opened, isOpen() is false and the
// caller falls back to simulation.
#pragma once
#include <cstdint>
#include "i8255_backend.hpp"

namespace traffic {

class DevPortIO : public PortIO {
public:
    DevPortIO();
    ~DevPortIO() override;
    bool isOpen() const override { return fd_ >= 0; }
    void write(std::uint16_t port, std::uint8_t value) override;
    std::uint8_t read(std::uint16_t port) override;
private:
    int fd_ = -1;
};

}  // namespace traffic
