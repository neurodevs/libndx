#include <catch2/catch_all.hpp>
#include <fcntl.h>
#include <unistd.h>
#include "ndx/usb_provider.hpp"

TEST_CASE("usb_port_path builds the macOS device path from the serial number") {
  REQUIRE(ndx::usb_port_path("ABCD1234") == "/dev/cu.usbserial-ABCD1234");
}

TEST_CASE("open_usb_serial_port returns -1 for a nonexistent path") {
  REQUIRE(ndx::open_usb_serial_port("/dev/cu.usbserial-DOESNOTEXIST") == -1);
}

TEST_CASE("open_usb_serial_port returns a valid descriptor for existing path") {
  int fd = ndx::open_usb_serial_port("/dev/null");
  REQUIRE(fd >= 0);
  REQUIRE(fcntl(fd, F_GETFD) != -1);
  close(fd);
}
