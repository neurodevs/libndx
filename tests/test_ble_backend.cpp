#include <catch2/catch_test_macros.hpp>
#include "ndx/ble_backend.hpp"

TEST_CASE("BleBackend can be instantiated") {
  ndx::BleBackend backend;
  REQUIRE(true);
}