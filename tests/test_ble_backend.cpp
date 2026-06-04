#include <catch2/catch_all.hpp>
#include <functional>
#include <unordered_map>
#include "ndx/acquisition_backend.hpp"
#include "ndx/ble_backend.hpp"

struct FakeBleProvider : ndx::BleProvider {
  bool powered_on = true;
  std::string scan_requested_for;
  std::vector<ndx::PeripheralInfo> scan_all_results;
  std::string disconnect_requested_for;

  std::string last_write_char_uuid;
  std::vector<uint8_t> last_write_data;

  std::unordered_map<std::string, std::function<void(const ndx::Packet&)>> callbacks;

  bool is_powered_on() override { return powered_on; }
  int read_rssi() override { return 0; }
  void set_rssi_interval(int, std::function<void(int)>) override {}
  void stop_rssi_interval() override {}
  void write_characteristic(const std::string& char_uuid, const uint8_t* data, size_t len) override {
    last_write_char_uuid = char_uuid;
    last_write_data.assign(data, data + len);
  }
  void scan_for_peripheral(const std::string& uuid, ndx::CharCallbacks cbs,  ndx::OnConnectedCallback) override {
    scan_requested_for = uuid;
    for (auto& e : cbs) callbacks[e.char_uuid] = std::move(e.on_data);
  }
  void scan_all(int duration_ms, ndx::ScanResultCallback cb) override {
    cb(scan_all_results);
  }
  void disconnect_peripheral(const std::string& uuid) override {
    disconnect_requested_for = uuid;
  }

  void simulate_packet(const ndx::Packet& p, const std::string& char_uuid = "") {
    auto it = callbacks.find(char_uuid);
    if (it != callbacks.end()) it->second(p);
  }
};

struct TestableBleBackend : ndx::BleBackend {
  using ndx::BleBackend::BleBackend;
  using ndx::AcquisitionBackend::is_intentional_disconnect;
};

struct BleBackendFixture {
  FakeBleProvider* provider;
  TestableBleBackend backend;

  BleBackendFixture()
    : provider(new FakeBleProvider()),
      backend("A1:B2:C3:D4:E5:F6", std::unique_ptr<ndx::BleProvider>(provider)) {}

  void start() {
    backend.start({});
  }

  void stop() {
    backend.stop();
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
  const std::string uuid = "test-char-uuid";
  backend.start({{uuid, std::nullopt, [&](const ndx::Packet&) { called = true; }}});
  provider->simulate_packet(ndx::Packet{}, uuid);
  REQUIRE(called);
}

TEST_CASE_METHOD(BleBackendFixture, "BleBackend only invokes callback for matching char UUID") {
  int a_calls = 0, b_calls = 0;
  backend.start({
    {"uuid-a", std::nullopt, [&](const ndx::Packet&) { a_calls++; }},
    {"uuid-b", std::nullopt, [&](const ndx::Packet&) { b_calls++; }},
  });
  provider->simulate_packet(ndx::Packet{}, "uuid-a");
  REQUIRE(a_calls == 1);
  REQUIRE(b_calls == 0);
}

TEST_CASE_METHOD(BleBackendFixture, "BleBackend does not invoke callback for unregistered char UUID") {
  bool called = false;
  backend.start({{"uuid-a", std::nullopt, [&](const ndx::Packet&) { called = true; }}});
  provider->simulate_packet(ndx::Packet{}, "uuid-b");
  REQUIRE_FALSE(called);
}

TEST_CASE_METHOD(BleBackendFixture, "BleBackend stop sets is_running to false") {
  start();
  stop();
  REQUIRE_FALSE(backend.is_running());
}

TEST_CASE_METHOD(BleBackendFixture, "BleBackend stop throws if not running") {
  REQUIRE_THROWS_WITH(stop(), "BleBackend: stop called while not running");
}

TEST_CASE_METHOD(BleBackendFixture, "BleBackend stop calls stop on provider") {
  start();
  stop();

  REQUIRE(provider->disconnect_requested_for == "A1:B2:C3:D4:E5:F6");
}

TEST_CASE_METHOD(BleBackendFixture, "BleBackend start throws if already running") {
  start();
  REQUIRE_THROWS_WITH(start(), "BleBackend: start called while already running");
}

TEST_CASE_METHOD(BleBackendFixture, "BleBackend start throws when Bluetooth is not powered on") {
  provider->powered_on = false;
  REQUIRE_THROWS_WITH(start(), "BleBackend: Bluetooth is not powered on");
}

TEST_CASE_METHOD(BleBackendFixture, "BleBackend start scans for peripheral with device_id") {
  start();
  REQUIRE(provider->scan_requested_for == "A1:B2:C3:D4:E5:F6");
}

TEST_CASE_METHOD(BleBackendFixture, "BleBackend scan_all returns discovered peripherals") {
  provider->scan_all_results = {
    {"AA:BB:CC:DD:EE:FF", "Muse-1234"},
    {"11:22:33:44:55:66", "Muse-5678"},
  };

  std::vector<ndx::PeripheralInfo> results;

  backend.scan_all(5000, [&](const std::vector<ndx::PeripheralInfo>& r) {
    results = r;
  });

  REQUIRE(results.size() == 2);
  REQUIRE(results[0].uuid == "AA:BB:CC:DD:EE:FF");
  REQUIRE(results[0].name == "Muse-1234");
  REQUIRE(results[1].uuid == "11:22:33:44:55:66");
  REQUIRE(results[1].name == "Muse-5678");
}

TEST_CASE_METHOD(BleBackendFixture, "BleBackend sets is_intentional_disconnect false") {
  REQUIRE_FALSE(backend.is_intentional_disconnect());
}

TEST_CASE_METHOD(BleBackendFixture, "BleBackend write_characteristic forwards data to provider") {
  const uint8_t data[] = {0x02, 'h', '\n'};
  backend.write_characteristic("273E0001-4C4D-454D-96BE-F03BAC821358", data, sizeof(data));
  REQUIRE(provider->last_write_char_uuid == "273E0001-4C4D-454D-96BE-F03BAC821358");
  REQUIRE(provider->last_write_data == std::vector<uint8_t>{0x02, 'h', '\n'});
}
