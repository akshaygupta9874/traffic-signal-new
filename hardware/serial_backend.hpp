// hardware/serial_backend.hpp - PC -> USB/serial -> microcontroller -> LEDs.
// Protocol (ASCII, one line per update):   T <G|Y|R|O> <mask 0..7>\n
//   G/Y/R/O = green/yellow/red/off, mask = LEFT 1 | STRAIGHT 2 | RIGHT 4.
// See hardware/arduino/traffic_signal.ino for the receiving side.
#pragma once
#include <chrono>
#include <string>
#include "hardware_interface.hpp"

namespace traffic {

class ByteSink {
public:
    virtual ~ByteSink() = default;
    virtual bool isOpen() const = 0;
    virtual bool write(const std::string& bytes) = 0;
};

// Collects output in memory (tests).
class StringByteSink : public ByteSink {
public:
    bool isOpen() const override { return true; }
    bool write(const std::string& b) override { data += b; return true; }
    std::string data;
};

// POSIX serial port, 9600 8N1 raw. Closed (isOpen()==false) if it cannot be opened.
class FileByteSink : public ByteSink {
public:
    explicit FileByteSink(const std::string& path);
    ~FileByteSink() override;
    bool isOpen() const override { return fd_ >= 0; }
    bool write(const std::string& bytes) override;
private:
    int fd_ = -1;
};

class SerialBackend : public HardwareInterface {
public:
    explicit SerialBackend(ByteSink& sink) : sink_(sink) {}
    const char* name() const override { return "serial"; }
    bool initialize() override { available_ = sink_.isOpen(); if (available_) allOff(); return available_; }
    bool isAvailable() const override { return available_; }
    void applyState(const TrafficState& s) override;
    void allOff() override { send('O', 0); }
    static std::string encode(const TrafficState& s);
private:
    void send(char c, unsigned mask);
    ByteSink& sink_;
    bool available_ = false;
    std::string lastLine_;
    std::chrono::steady_clock::time_point lastSent_{};
};

}  // namespace traffic
