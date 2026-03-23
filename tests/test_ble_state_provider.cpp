#include <catch2/catch_all.hpp>
#include "ndx/ble_state_provider.hpp"

TEST_CASE("createBleStateProvider returns a provider") {
  REQUIRE(ndx::createBleStateProvider() != nullptr);
}
