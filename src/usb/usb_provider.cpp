#include "ndx/usb_provider.hpp"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <termios.h>
#include <thread>
#include <unistd.h>

namespace ndx {

static thread_local int g_last_open_errno = 0;

std::function<int(const char*, int)> UsbProviderSyscalls::open =
    [](const char* path, int flags) { return ::open(path, flags); };
std::function<int(int)> UsbProviderSyscalls::close = ::close;
std::function<int(int, int, const termios*)> UsbProviderSyscalls::tcsetattr = ::tcsetattr;
std::function<void(std::chrono::milliseconds)> UsbProviderSyscalls::sleep_for =
    [](std::chrono::milliseconds d) { std::this_thread::sleep_for(d); };
std::function<ssize_t(int, const uint8_t*, size_t)> UsbProviderSyscalls::write =
    [](int fd, const uint8_t* data, size_t len) { return ::write(fd, data, len); };

namespace {

class PosixUsbProvider : public UsbProvider {
public:
  void connect(const std::string& device_id, OnDataCallback on_data, OnConnectedCallback on_connected, int waitAfterConnectMs) override {
    fd_ = open_usb_serial_port(usb_port_path(device_id), B115200);

    if (fd_ < 0) {
      throw std::runtime_error("UsbProvider: could not open port for device " + device_id +
                                " (" + strerror(g_last_open_errno) + ")");
    }

    if (waitAfterConnectMs > 0) {
      UsbProviderSyscalls::sleep_for(std::chrono::milliseconds(waitAfterConnectMs));
    }

    tcflush(fd_, TCIFLUSH);

    if (on_connected) {
      Device device{device_id, device_id};
      on_connected(&device);
    }

    running_ = true;
    
    read_thread_ = std::thread([this, on_data = std::move(on_data)]() {
      while (running_) {
        read_available_data(fd_, on_data);
        UsbProviderSyscalls::sleep_for(std::chrono::milliseconds(1));
      }
    });
  }

  void disconnect() override {
    running_ = false;
    if (read_thread_.joinable()) read_thread_.join();
    if (fd_ >= 0) {
      UsbProviderSyscalls::close(fd_);
      fd_ = -1;
    }
  }

  bool write(const uint8_t* data, size_t len) override {
    return write_usb_serial_port(fd_, data, len);
  }

private:
  int fd_ = -1;
  std::atomic<bool> running_{false};
  std::thread read_thread_;
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
  if (fd < 0) {
    g_last_open_errno = errno;
    return -1;
  }

  termios tty{};
  if (tcgetattr(fd, &tty) != 0) {
    g_last_open_errno = errno;
    UsbProviderSyscalls::close(fd);
    return -1;
  }

  cfmakeraw(&tty);
  cfsetispeed(&tty, baud);
  cfsetospeed(&tty, baud);

  if (UsbProviderSyscalls::tcsetattr(fd, TCSANOW, &tty) != 0) {
    g_last_open_errno = errno;
    UsbProviderSyscalls::close(fd);
    return -1;
  }

  return fd;
}

void read_available_data(int fd, OnDataCallback on_data) {
  char buf[256];
  ssize_t n = read(fd, buf, sizeof(buf));
  
  if (n > 0 && on_data) {
    Packet p;
    p.data.assign(buf, buf + n);
    on_data(p);
  }
}

bool write_usb_serial_port(int fd, const uint8_t* data, size_t len) {
  return UsbProviderSyscalls::write(fd, data, len) == static_cast<ssize_t>(len);
}

}
