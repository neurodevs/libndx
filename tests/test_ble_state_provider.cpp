#include <catch2/catch_all.hpp>
#include "ndx/ble_state_provider.hpp"

TEST_CASE("createBleStateProvider returns a provider") {
  REQUIRE(ndx::createBleStateProvider() != nullptr);
}

TEST_CASE("BleStateProvider reports powered on", "[integration]") {
  auto provider = ndx::createBleStateProvider();
  if (!provider->isPoweredOn()) {
    SKIP("Please ensure Bluetooth is powered on to run this test");
  }
  REQUIRE(provider->isPoweredOn());
}

