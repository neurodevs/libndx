#include "ndx/usb_provider.hpp"

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

namespace ndx {

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

int open_usb_serial_port(const std::string& path) {
  int fd = open(path.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fd < 0) return -1;
  return fd;
}

}
