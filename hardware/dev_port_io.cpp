#include "dev_port_io.hpp"

#if defined(__linux__)
#include <fcntl.h>
#include <unistd.h>
#endif

namespace traffic {

#if defined(__linux__)
DevPortIO::DevPortIO() { fd_ = ::open("/dev/port", O_RDWR); }
DevPortIO::~DevPortIO() { if (fd_ >= 0) ::close(fd_); }
void DevPortIO::write(std::uint16_t port, std::uint8_t value) {
    if (fd_ >= 0 && ::pwrite(fd_, &value, 1, static_cast<off_t>(port)) != 1) { /* write failed: nothing more to do */ }
}
std::uint8_t DevPortIO::read(std::uint16_t port) {
    std::uint8_t v = 0xFF;
    if (fd_ >= 0 && ::pread(fd_, &v, 1, static_cast<off_t>(port)) != 1) v = 0xFF;
    return v;
}
#else
DevPortIO::DevPortIO() {}                       // unsupported platform: stays closed
DevPortIO::~DevPortIO() {}
void DevPortIO::write(std::uint16_t, std::uint8_t) {}
std::uint8_t DevPortIO::read(std::uint16_t) { return 0xFF; }
#endif

}  // namespace traffic
