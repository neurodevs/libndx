#include <catch2/catch_all.hpp>
#include "ndx/ble_backend.hpp"

struct FakeBleStateProvider : ndx::BleStateProvider {
  bool powered_on = true;
  bool isPoweredOn() override { return powered_on; }
};

struct TestableBleBackend : ndx::BleBackend {
  using ndx::BleBackend::BleBackend;
  void simulatePacket(const ndx::Packet& p) { fireCallback(p); }
};

struct BleBackendFixture {
  FakeBleStateProvider* provider;
  TestableBleBackend backend;

  BleBackendFixture()
    : provider(new FakeBleStateProvider()),
      backend("A1:B2:C3:D4:E5:F6", std::unique_ptr<ndx::BleStateProvider>(provider)) {}

  void start() {
    backend.start([](const ndx::Packet&) {});
  }

  void stop() {
    backend.stop();
  }

  void destroy() {
    backend.destroy();
  }
};

TEST_CASE_METHOD(BleBackendFixture, "BleBackend can be instantiated") {
  REQUIRE(true);
}

TEST_CASE_METHOD(BleBackendFixture, "BleBackend start sets is_running to true") {
  start();
  REQUIRE(backend.is_running());
}

TEST_CASE_METHOD(BleBackendFixture, "BleBackend invokes callback when packet received") {
  bool called = false;
  backend.start([&](const ndx::Packet& p) {
      called = true;
  });
  backend.simulatePacket(ndx::Packet{});
  REQUIRE(called);
}

TEST_CASE_METHOD(BleBackendFixture, "BleBackend stop sets is_running to false") {
  start();
  stop();
  REQUIRE_FALSE(backend.is_running());
}

TEST_CASE_METHOD(BleBackendFixture, "BleBackend destroy works if not running") {
  destroy();
  REQUIRE_FALSE(backend.is_running());
}

TEST_CASE_METHOD(BleBackendFixture, "BleBackend destroy sets is_running to false") {
  start();
  destroy();
  REQUIRE_FALSE(backend.is_running());
}

TEST_CASE_METHOD(BleBackendFixture, "BleBackend start throws if already running") {
  start();
  REQUIRE_THROWS_WITH(start(), "BleBackend: start called while already running");
}

TEST_CASE_METHOD(BleBackendFixture, "BleBackend stop throws if not running") {
  REQUIRE_THROWS_WITH(stop(), "BleBackend: stop called while not running");
}

TEST_CASE_METHOD(BleBackendFixture, "BleBackend start throws when Bluetooth is not powered on") {
  provider->powered_on = false;
  REQUIRE_THROWS_WITH(start(), "BleBackend: Bluetooth is not powered on");
}