#pragma once
#include "acquisition_backend.hpp"
#include <functional>
#include <memory>
#include <string>
#include <termios.h>

namespace ndx {

class UsbProvider {
public:
  virtual ~UsbProvider() = default;
  virtual void connect(const std::string& device_id,
                       OnDataCallback on_data,
                       OnConnectedCallback on_connected) = 0;
  virtual void disconnect() = 0;
};

std::unique_ptr<UsbProvider> create_usb_provider();

std::string usb_port_path(const std::string& serial_number);

class UsbProviderSyscalls {
public:
  static std::function<int(int)> close;
  static std::function<int(int, int, const termios*)> tcsetattr;
};

int open_usb_serial_port(const std::string& path, speed_t baud);

}
