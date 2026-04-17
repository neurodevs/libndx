#include <catch2/catch_all.hpp>
#include "ndx/ble_provider.hpp"

TEST_CASE("create_ble_provider returns a provider") {
  REQUIRE(ndx::create_ble_provider() != nullptr);
}

TEST_CASE("BleProvider reports powered on", "[integration]") {
  auto provider = ndx::create_ble_provider();
  if (!provider->is_powered_on()) {
    SKIP("Please ensure Bluetooth is powered on to run this test");
  }
  REQUIRE(provider->is_powered_on());
}
