#include "ndx/usb_provider.hpp"

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

}
