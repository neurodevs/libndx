#include "ndx/usb_provider.hpp"

#include <lsl_c.h>

#include <IOKit/serial/ioss.h>
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <stdexcept>
#include <sys/ioctl.h>
#include <termios.h>
#include <thread>
#include <unistd.h>

namespace ndx {

static thread_local int g_last_open_errno = 0;

std::function<int(const char*, int)> UsbProviderSyscalls::open =
    [](const char* path, int flags) { return ::open(path, flags); };
std::function<int(int)> UsbProviderSyscalls::close = ::close;
std::function<int(int, int, const termios*)> UsbProviderSyscalls::tcsetattr = ::tcsetattr;
std::function<int(int, speed_t)> UsbProviderSyscalls::set_nonstandard_baud_rate =
    [](int fd, speed_t baud) { return ::ioctl(fd, IOSSIOSPEED, &baud); };
std::function<std::vector<std::string>()> UsbProviderSyscalls::list_dev_names = []() {
  std::vector<std::string> names;
  DIR* dir = opendir("/dev");
  if (!dir) return names;
  while (struct dirent* entry = readdir(dir)) {
    names.emplace_back(entry->d_name);
  }
  closedir(dir);
  return names;
};
std::function<void(std::chrono::milliseconds)> UsbProviderSyscalls::sleep_for =
    [](std::chrono::milliseconds d) { std::this_thread::sleep_for(d); };
std::function<ssize_t(int, const uint8_t*, size_t)> UsbProviderSyscalls::write =
    [](int fd, const uint8_t* data, size_t len) { return ::write(fd, data, len); };
std::function<double()> UsbProviderSyscalls::now = []() { return lsl_local_clock(); };

namespace {

class PosixUsbProvider : public UsbProvider {
public:
  void connect(const std::string& device_id, OnDataCallback on_data, OnConnectedCallback on_connected, int wait_after_connect_ms) override {
    fd_ = open_usb_serial_port(usb_port_path(device_id), baud_rate_, uses_rts_cts_);

    if (fd_ < 0) {
      throw std::runtime_error("UsbProvider: could not open port for device " + device_id +
                                " (" + strerror(g_last_open_errno) + ")");
    }

    tcflush(fd_, TCIFLUSH);

    if (wait_after_connect_ms > 0) {
      UsbProviderSyscalls::sleep_for(std::chrono::milliseconds(wait_after_connect_ms));
    }

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

  void configure_port(speed_t baud_rate, bool uses_rts_cts) override {
    baud_rate_ = baud_rate;
    uses_rts_cts_ = uses_rts_cts;
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
  speed_t baud_rate_ = B115200;
  bool uses_rts_cts_ = false;
  std::atomic<bool> running_{false};
  std::thread read_thread_;
};

}

std::unique_ptr<UsbProvider> create_usb_provider() {
  return std::make_unique<PosixUsbProvider>();
}

static const std::string kUsbPortNamePrefix = "cu.usbserial-";

std::string usb_port_path(const std::string& serial_number) {
  return "/dev/" + kUsbPortNamePrefix + serial_number;
}

std::vector<std::string> list_usb_serial_numbers() {
  std::vector<std::string> serial_numbers;

  for (const auto& name : UsbProviderSyscalls::list_dev_names()) {
    if (name.rfind(kUsbPortNamePrefix, 0) == 0) {
      serial_numbers.push_back(name.substr(kUsbPortNamePrefix.size()));
    }
  }

  std::sort(serial_numbers.begin(), serial_numbers.end());
  return serial_numbers;
}

static bool is_standard_baud_rate(speed_t baud) {
  static const speed_t standard[] = {B9600, B19200, B38400, B57600, B115200, B230400};
  return std::find(std::begin(standard), std::end(standard), baud) != std::end(standard);
}

int open_usb_serial_port(const std::string& path, speed_t baud, bool uses_rts_cts) {
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

  const bool is_standard = is_standard_baud_rate(baud);
  const speed_t termios_baud = is_standard ? baud : B115200;

  cfmakeraw(&tty);
  cfsetispeed(&tty, termios_baud);
  cfsetospeed(&tty, termios_baud);

  if (uses_rts_cts) {
    tty.c_cflag |= CRTSCTS;
  }

  if (UsbProviderSyscalls::tcsetattr(fd, TCSANOW, &tty) != 0) {
    g_last_open_errno = errno;
    UsbProviderSyscalls::close(fd);
    return -1;
  }

  // termios on macOS only accepts the standard rates: any other one has to
  // be set afterwards, since tcsetattr resets the speed.
  if (!is_standard && UsbProviderSyscalls::set_nonstandard_baud_rate(fd, baud) != 0) {
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
    p.timestamp_sec = UsbProviderSyscalls::now();
    p.data.assign(buf, buf + n);
    on_data(p);
  }
}

bool write_usb_serial_port(int fd, const uint8_t* data, size_t len) {
  return UsbProviderSyscalls::write(fd, data, len) == static_cast<ssize_t>(len);
}

}
