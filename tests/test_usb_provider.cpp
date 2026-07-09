#include <catch2/catch_all.hpp>
#include <chrono>
#include <cstdlib>
#include <fcntl.h>
#include <mutex>
#include <random>
#include <termios.h>
#include <thread>
#include <unistd.h>
#include <util.h>
#include <vector>
#include "ndx/usb_provider.hpp"

namespace {

struct Pty {
  int master_fd;
  char slave_path[64];

  Pty() {
    int slave_fd;
    REQUIRE(openpty(&master_fd, &slave_fd, slave_path, nullptr, nullptr) == 0);
    close(slave_fd);
  }

  ~Pty() { close(master_fd); }
};

struct RestoreUsbProviderSyscalls {
  ~RestoreUsbProviderSyscalls() {
    ndx::UsbProviderSyscalls::open = [](const char* path, int flags) { return ::open(path, flags); };
    ndx::UsbProviderSyscalls::close = ::close;
    ndx::UsbProviderSyscalls::tcsetattr = ::tcsetattr;
    ndx::UsbProviderSyscalls::sleep_for = [](std::chrono::milliseconds d) { std::this_thread::sleep_for(d); };
    ndx::UsbProviderSyscalls::write = [](int fd, const uint8_t* data, size_t len) { return ::write(fd, data, len); };
  }
};

struct DisconnectGuard {
  ndx::UsbProvider* provider;
  ~DisconnectGuard() { provider->disconnect(); }
};

}

TEST_CASE("usb_port_path builds the macOS device path from the serial number") {
  REQUIRE(ndx::usb_port_path("ABCD1234") == "/dev/cu.usbserial-ABCD1234");
}

TEST_CASE("open_usb_serial_port returns -1 for a nonexistent path") {
  REQUIRE(ndx::open_usb_serial_port("/dev/cu.usbserial-DOESNOTEXIST", B115200) == -1);
}

TEST_CASE("open_usb_serial_port closes the fd when tcgetattr fails") {
  RestoreUsbProviderSyscalls restore;
  std::vector<int> closed_fds;
  ndx::UsbProviderSyscalls::close = [&](int fd) {
    closed_fds.push_back(fd);
    return ::close(fd);
  };

  char tmpl[] = "/tmp/ndx_usb_provider_test_XXXXXX";
  int tmp_fd = mkstemp(tmpl);
  REQUIRE(tmp_fd >= 0);
  ::close(tmp_fd);

  REQUIRE(ndx::open_usb_serial_port(tmpl, B115200) == -1);
  REQUIRE(closed_fds.size() == 1);
  REQUIRE(closed_fds[0] >= 0);

  unlink(tmpl);
}

TEST_CASE("open_usb_serial_port closes the fd when tcsetattr fails") {
  RestoreUsbProviderSyscalls restore;
  std::vector<int> closed_fds;
  ndx::UsbProviderSyscalls::close = [&](int fd) {
    closed_fds.push_back(fd);
    return ::close(fd);
  };
  ndx::UsbProviderSyscalls::tcsetattr = [](int, int, const termios*) { return -1; };

  Pty pty;
  REQUIRE(ndx::open_usb_serial_port(pty.slave_path, B115200) == -1);
  REQUIRE(closed_fds.size() == 1);
  REQUIRE(closed_fds[0] >= 0);
}

TEST_CASE("open_usb_serial_port returns a valid descriptor for existing path") {
  Pty pty;
  int fd = ndx::open_usb_serial_port(pty.slave_path, B115200);
  REQUIRE(fd >= 0);
  close(fd);
}

TEST_CASE("UsbProvider connect throws when port can't be opened") {
  auto provider = ndx::create_usb_provider();
  REQUIRE_THROWS(provider->connect("DOESNOTEXIST", [](const ndx::Packet&) {}, nullptr));
}

TEST_CASE("UsbProvider disconnect closes the fd opened by connect") {
  RestoreUsbProviderSyscalls restore;
  Pty pty;

  ndx::UsbProviderSyscalls::open = [&](const char*, int flags) {
    return ::open(pty.slave_path, flags);
  };

  std::vector<int> closed_fds;
  ndx::UsbProviderSyscalls::close = [&](int fd) {
    closed_fds.push_back(fd);
    return ::close(fd);
  };

  auto provider = ndx::create_usb_provider();
  provider->connect("ABCD1234", [](const ndx::Packet&) {}, nullptr);

  provider->disconnect();

  REQUIRE(closed_fds.size() == 1);
}

TEST_CASE("UsbProvider write sends bytes to the connected device") {
  RestoreUsbProviderSyscalls restore;
  Pty pty;

  ndx::UsbProviderSyscalls::open = [&](const char*, int flags) {
    return ::open(pty.slave_path, flags);
  };

  auto provider = ndx::create_usb_provider();
  provider->connect("ABCD1234", [](const ndx::Packet&) {}, nullptr);
  DisconnectGuard guard{provider.get()};

  const std::string sent = "v";
  REQUIRE(provider->write(reinterpret_cast<const uint8_t*>(sent.data()), sent.size()));

  char buf[16] = {};
  ssize_t n = read(pty.master_fd, buf, sizeof(buf));
  REQUIRE(n == (ssize_t)sent.size());
  REQUIRE(std::string(buf, n) == sent);
}

TEST_CASE("UsbProvider connect invokes on_connected on success") {
  RestoreUsbProviderSyscalls restore;
  Pty pty;

  ndx::UsbProviderSyscalls::open = [&](const char*, int flags) {
    return ::open(pty.slave_path, flags);
  };

  auto provider = ndx::create_usb_provider();
  bool connected = false;
  std::string connected_id;
  provider->connect("ABCD1234", [](const ndx::Packet&) {},
                     [&](const ndx::Device* device) {
                       connected = device != nullptr;
                       if (device) connected_id = device->id;
                     });

  REQUIRE(connected);
  REQUIRE(connected_id == "ABCD1234");

  provider->disconnect();
}

TEST_CASE("UsbProvider connect delivers incoming bytes via on_data") {
  RestoreUsbProviderSyscalls restore;
  Pty pty;

  ndx::UsbProviderSyscalls::open = [&](const char*, int flags) {
    return ::open(pty.slave_path, flags);
  };

  auto provider = ndx::create_usb_provider();
  std::vector<uint8_t> received;
  provider->connect("ABCD1234", [&](const ndx::Packet& p) {
    received.insert(received.end(), p.data.begin(), p.data.end());
  }, nullptr);

  const std::string sent = "hello";
  REQUIRE(write(pty.master_fd, sent.data(), sent.size()) == (ssize_t)sent.size());

  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (received.size() < sent.size() && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  REQUIRE(std::string(received.begin(), received.end()) == sent);

  provider->disconnect();
}

TEST_CASE("UsbProvider connect throttles its read loop with a 1ms sleep") {
  RestoreUsbProviderSyscalls restore;
  Pty pty;

  ndx::UsbProviderSyscalls::open = [&](const char*, int flags) {
    return ::open(pty.slave_path, flags);
  };

  std::vector<std::chrono::milliseconds> sleeps;
  std::mutex sleeps_mutex;
  ndx::UsbProviderSyscalls::sleep_for = [&](std::chrono::milliseconds d) {
    std::lock_guard<std::mutex> lock(sleeps_mutex);
    sleeps.push_back(d);
  };

  auto provider = ndx::create_usb_provider();
  provider->connect("ABCD1234", [](const ndx::Packet&) {}, nullptr);

  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (std::chrono::steady_clock::now() < deadline) {
    std::lock_guard<std::mutex> lock(sleeps_mutex);
    if (!sleeps.empty()) break;
  }

  provider->disconnect();

  std::lock_guard<std::mutex> lock(sleeps_mutex);
  REQUIRE_FALSE(sleeps.empty());
  REQUIRE(sleeps.front() == std::chrono::milliseconds(1));
}

TEST_CASE("write_usb_serial_port writes bytes to the fd") {
  Pty pty;
  int fd = ndx::open_usb_serial_port(pty.slave_path, B115200);
  REQUIRE(fd >= 0);

  const std::string sent = "v";
  REQUIRE(ndx::write_usb_serial_port(fd, reinterpret_cast<const uint8_t*>(sent.data()), sent.size()));

  char buf[16] = {};
  ssize_t n = read(pty.master_fd, buf, sizeof(buf));
  REQUIRE(n == (ssize_t)sent.size());
  REQUIRE(std::string(buf, n) == sent);

  close(fd);
}

TEST_CASE("read_available_data does not invoke on_data when nothing is available") {
  Pty pty;
  int fd = ndx::open_usb_serial_port(pty.slave_path, B115200);
  REQUIRE(fd >= 0);

  bool called = false;
  ndx::read_available_data(fd, [&](const ndx::Packet&) { called = true; });

  REQUIRE_FALSE(called);

  close(fd);
}

TEST_CASE("read_available_data invokes on_data with bytes read from the fd") {
  Pty pty;
  int fd = ndx::open_usb_serial_port(pty.slave_path, B115200);
  REQUIRE(fd >= 0);

  const std::string sent = "hello";
  REQUIRE(write(pty.master_fd, sent.data(), sent.size()) == (ssize_t)sent.size());

  std::vector<uint8_t> received;
  ndx::read_available_data(fd, [&](const ndx::Packet& p) {
    received.insert(received.end(), p.data.begin(), p.data.end());
  });

  REQUIRE(std::string(received.begin(), received.end()) == sent);

  close(fd);
}

TEST_CASE("open_usb_serial_port puts port into raw mode") {
  Pty pty;
  int fd = ndx::open_usb_serial_port(pty.slave_path, B115200);
  REQUIRE(fd >= 0);

  termios tty{};
  REQUIRE(tcgetattr(fd, &tty) == 0);
  REQUIRE_FALSE(tty.c_lflag & ICANON);

  close(fd);
}

TEST_CASE("open_usb_serial_port configures the port at the requested baud rate") {
  static const speed_t kCandidateBauds[] = {B9600, B19200, B38400, B57600, B230400};
  std::mt19937 rng(std::random_device{}());
  std::uniform_int_distribution<size_t> dist(0, std::size(kCandidateBauds) - 1);
  speed_t baud = kCandidateBauds[dist(rng)];

  Pty pty;
  int fd = ndx::open_usb_serial_port(pty.slave_path, baud);
  REQUIRE(fd >= 0);

  termios tty{};
  REQUIRE(tcgetattr(fd, &tty) == 0);
  REQUIRE(cfgetispeed(&tty) == baud);
  REQUIRE(cfgetospeed(&tty) == baud);

  close(fd);
}
