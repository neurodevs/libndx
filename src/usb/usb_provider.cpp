#include "ndx/usb_provider.hpp"

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

namespace ndx {

std::function<int(int)> UsbProviderSyscalls::close = ::close;
std::function<int(int, int, const termios*)> UsbProviderSyscalls::tcsetattr = ::tcsetattr;

namespace {

class NullUsbProvider : public UsbProvider {
public:
  void connect(const std::string&, OnDataCallback, OnConnectedCallback) override {}
  void disconnect() override {}
};

}

std::unique_ptr<UsbProvider> create_usb_provider() {
  return std::make_unique<NullUsbProvider>();
}

std::string usb_port_path(const std::string& serial_number) {
  return "/dev/cu.usbserial-" + serial_number;
}

int open_usb_serial_port(const std::string& path, speed_t baud) {
  int fd = open(path.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
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
