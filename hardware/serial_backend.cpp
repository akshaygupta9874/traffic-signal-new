#include "serial_backend.hpp"

#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace traffic {

std::string SerialBackend::encode(const TrafficState& s) {
    char c = 'O';
    unsigned mask = 0;
    switch (s.state) {
        case LightState::GREEN:  c = 'G'; break;
        case LightState::YELLOW: c = 'Y'; break;
        case LightState::RED:    c = 'R'; break;
        default: break;
    }
    if (s.state != LightState::OFF) mask = toMask(s.direction);
    return std::string("T ") + c + " " + static_cast<char>('0' + mask) + "\n";
}

void SerialBackend::send(char c, unsigned mask) {
    if (!available_) return;
    lastLine_ = std::string("T ") + c + " " + static_cast<char>('0' + (mask & 7u)) + "\n";
    lastSent_ = std::chrono::steady_clock::now();
    sink_.write(lastLine_);
}

// Sends when the line changed, or at least once per second as a heartbeat
// (the Arduino blanks its LEDs if the link goes silent).
void SerialBackend::applyState(const TrafficState& s) {
    if (!available_) return;
    std::string line = encode(s);
    auto now = std::chrono::steady_clock::now();
    if (line != lastLine_ || now - lastSent_ >= std::chrono::seconds(1)) {
        lastLine_ = line;
        lastSent_ = now;
        sink_.write(line);
    }
}

#if defined(__unix__) || defined(__APPLE__)
FileByteSink::FileByteSink(const std::string& path) {
    fd_ = ::open(path.c_str(), O_RDWR | O_NOCTTY);
    if (fd_ < 0) return;
    termios tio{};
    if (tcgetattr(fd_, &tio) == 0) {
        cfmakeraw(&tio);
        cfsetispeed(&tio, B9600);
        cfsetospeed(&tio, B9600);
        tcsetattr(fd_, TCSANOW, &tio);
    }
}
FileByteSink::~FileByteSink() { if (fd_ >= 0) ::close(fd_); }
bool FileByteSink::write(const std::string& b) {
    return fd_ >= 0 && ::write(fd_, b.data(), b.size()) == static_cast<ssize_t>(b.size());
}
#else
FileByteSink::FileByteSink(const std::string&) {}
FileByteSink::~FileByteSink() {}
bool FileByteSink::write(const std::string&) { return false; }
#endif

}  // namespace traffic
