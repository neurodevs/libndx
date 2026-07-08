#include <catch2/catch_all.hpp>
#include "ndx/usb_provider.hpp"

TEST_CASE("usb_port_path builds the macOS device path from the serial number") {
  REQUIRE(ndx::usb_port_path("ABCD1234") == "/dev/cu.usbserial-ABCD1234");
}
