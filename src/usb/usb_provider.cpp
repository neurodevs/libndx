#include "ndx/usb_provider.hpp"

#include <fcntl.h>
#include <stdexcept>
#include <termios.h>
#include <unistd.h>

namespace ndx {

std::function<int(const char*, int)> UsbProviderSyscalls::open =
    [](const char* path, int flags) { return ::open(path, flags); };
std::function<int(int)> UsbProviderSyscalls::close = ::close;
std::function<int(int, int, const termios*)> UsbProviderSyscalls::tcsetattr = ::tcsetattr;

namespace {

class PosixUsbProvider : public UsbProvider {
public:
  void connect(const std::string& device_id, OnDataCallback, OnConnectedCallback) override {
    fd_ = open_usb_serial_port(usb_port_path(device_id), B115200);

    if (fd_ < 0) {
      throw std::runtime_error("UsbProvider: could not open port for device " + device_id);
    }
  }

  void disconnect() override {
    if (fd_ >= 0) {
      UsbProviderSyscalls::close(fd_);
      fd_ = -1;
    }
  }

private:
  int fd_ = -1;
};

}

std::unique_ptr<UsbProvider> create_usb_provider() {
  return std::make_unique<PosixUsbProvider>();
}

std::string usb_port_path(const std::string& serial_number) {
  return "/dev/cu.usbserial-" + serial_number;
}

int open_usb_serial_port(const std::string& path, speed_t baud) {
  int fd = UsbProviderSyscalls::open(path.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fd < 0) return -1;

  termios tty{};
  if (tcgetattr(fd, &tty) != 0) {
    UsbProviderSyscalls::close(fd);
    return -1;
  }

  cfsetispeed(&tty, baud);
  cfsetospeed(&tty, baud);

  if (UsbProviderSyscalls::tcsetattr(fd, TCSANOW, &tty) != 0) {
    UsbProviderSyscalls::close(fd);
    return -1;
  }

  return fd;
}

}
