#include <catch2/catch_test_macros.hpp>
#include "ndx/ble_backend.hpp"

struct BleBackendFixture {
  ndx::BleBackend backend{ "A1:B2:C3:D4:E5:F6" };
};

TEST_CASE_METHOD(BleBackendFixture, "BleBackend can be instantiated") {
  REQUIRE(true);
}
