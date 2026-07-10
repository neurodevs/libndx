#pragma once
#include "acquisition_backend.hpp"
#include <chrono>
#include <cstdint>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <sys/types.h>
#include <termios.h>

namespace ndx {

class UsbProvider {
public:
  virtual ~UsbProvider() = default;
  virtual void connect(const std::string& device_id,
                       OnDataCallback on_data,
                       OnConnectedCallback on_connected,
                       int wait_after_connect_ms = 0) = 0;
  virtual void disconnect() = 0;
  virtual bool write(const uint8_t* data, size_t len) = 0;
};

std::unique_ptr<UsbProvider> create_usb_provider();

std::string usb_port_path(const std::string& serial_number);

class UsbProviderSyscalls {
public:
  static std::function<int(const char*, int)> open;
  static std::function<int(int)> close;
  static std::function<int(int, int, const termios*)> tcsetattr;
  static std::function<void(std::chrono::milliseconds)> sleep_for;
  static std::function<ssize_t(int, const uint8_t*, size_t)> write;
};

int open_usb_serial_port(const std::string& path, speed_t baud);

void read_available_data(int fd, OnDataCallback on_data);

bool write_usb_serial_port(int fd, const uint8_t* data, size_t len);

}
