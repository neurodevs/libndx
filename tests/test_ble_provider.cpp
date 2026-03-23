#include <catch2/catch_all.hpp>
#include "ndx/ble_provider.hpp"

TEST_CASE("createBleProvider returns a provider") {
  REQUIRE(ndx::createBleProvider() != nullptr);
}

TEST_CASE("BleProvider reports powered on", "[integration]") {
  auto provider = ndx::createBleProvider();
  if (!provider->isPoweredOn()) {
    SKIP("Please ensure Bluetooth is powered on to run this test");
  }
  REQUIRE(provider->isPoweredOn());
}
