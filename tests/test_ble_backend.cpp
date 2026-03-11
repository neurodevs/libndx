#include <catch2/catch_all.hpp>
#include "ndx/ble_backend.hpp"

struct BleBackendFixture {
  ndx::BleBackend backend{ "A1:B2:C3:D4:E5:F6" };
};

TEST_CASE_METHOD(BleBackendFixture, "BleBackend can be instantiated") {
  REQUIRE(true);
}

TEST_CASE_METHOD(BleBackendFixture, "BleBackend start sets is_running to true") {
  backend.start([](const ndx::Packet&) {});
  REQUIRE(backend.is_running());
}

TEST_CASE_METHOD(BleBackendFixture, "BleBackend stop sets is_running to false") {
  backend.start([](const ndx::Packet&) {});
  backend.stop();
  REQUIRE_FALSE(backend.is_running());
}

TEST_CASE_METHOD(BleBackendFixture, "BleBackend stop throws if not running") {
  REQUIRE_THROWS_WITH(backend.stop(), "BleBackend: stop called while not running");
}