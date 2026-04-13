#include <catch2/catch_all.hpp>
#include "ndx/ble_backend.hpp"

struct FakeBleProvider : ndx::BleProvider {
  bool powered_on = true;
  std::string scan_requested_for;
  std::vector<ndx::PeripheralInfo> scan_all_results;

  bool isPoweredOn() override { return powered_on; }
  int getRssi() override { return 0; }
  void scanForPeripheral(const std::string& id, ndx::OnDataCallback) override {
    scan_requested_for = id;
  }
  void scanAll(int duration_ms, ndx::ScanResultCallback cb) override {
    cb(scan_all_results);
  }
};

struct TestableBleBackend : ndx::BleBackend {
  using ndx::BleBackend::BleBackend;
  void simulatePacket(const ndx::Packet& p) { fireCallback(p); }
};

struct BleBackendFixture {
  FakeBleProvider* provider;
  TestableBleBackend backend;

  BleBackendFixture()
    : provider(new FakeBleProvider()),
      backend("A1:B2:C3:D4:E5:F6", std::unique_ptr<ndx::BleProvider>(provider)) {}

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

TEST_CASE_METHOD(BleBackendFixture, "BleBackend start scans for peripheral with device_id") {
  start();
  REQUIRE(provider->scan_requested_for == "A1:B2:C3:D4:E5:F6");
}

TEST_CASE_METHOD(BleBackendFixture, "BleBackend scanAll returns discovered peripherals") {
  provider->scan_all_results = {
    {"AA:BB:CC:DD:EE:FF", "Muse-1234"},
    {"11:22:33:44:55:66", "Muse-5678"},
  };

  std::vector<ndx::PeripheralInfo> results;
  backend.scanAll(5000, [&](const std::vector<ndx::PeripheralInfo>& r) {
    results = r;
  });

  REQUIRE(results.size() == 2);
  REQUIRE(results[0].id == "AA:BB:CC:DD:EE:FF");
  REQUIRE(results[0].name == "Muse-1234");
  REQUIRE(results[1].id == "11:22:33:44:55:66");
  REQUIRE(results[1].name == "Muse-5678");
}

TEST_CASE_METHOD(BleBackendFixture, "BleBackend sets isIntentionalDisconnect false") {
  REQUIRE_FALSE(backend.isIntentionalDisconnect());
}