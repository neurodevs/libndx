#include <catch2/catch_all.hpp>
#include <functional>
#include <unordered_map>
#include "ndx/acquisition_backend.hpp"
#include "ndx/ble_gatt_backend.hpp"

struct FakeBleProvider : ndx::BleProvider {
  bool powered_on = true;
  std::string scan_requested_for;
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

  ndx::OnDisconnectedCallback on_disconnected;

  void scan_for_peripheral(const std::string& uuid, ndx::CharCallbacks cbs,  ndx::OnConnectedCallback,
                           ndx::OnDisconnectedCallback on_disconnected_cb) override {
    scan_requested_for = uuid;
    on_disconnected = std::move(on_disconnected_cb);
    for (auto& e : cbs) callbacks[e.char_uuid] = std::move(e.on_data);
  }

  void simulate_unexpected_disconnect() {
    if (on_disconnected) on_disconnected(false);
  }

  bool add_char_callbacks_called = false;

  void add_char_callbacks(ndx::CharCallbacks cbs) override {
    add_char_callbacks_called = true;
    for (auto& e : cbs) callbacks[e.char_uuid] = std::move(e.on_data);
  }

  void discover_ble_uuid(const std::string&, std::function<void(const std::string&)>) override {}
  
  void disconnect_peripheral(const std::string& uuid) override {
    disconnect_requested_for = uuid;
  }

  void simulate_packet(const ndx::Packet& p, const std::string& char_uuid = "") {
    auto it = callbacks.find(char_uuid);
    if (it != callbacks.end()) it->second(p);
  }
};

struct TestableBleGattBackend : ndx::BleGattBackend {
  using ndx::BleGattBackend::BleGattBackend;
  using ndx::AcquisitionBackend::is_intentional_disconnect;
};

struct BleGattBackendFixture {
  FakeBleProvider* provider;
  TestableBleGattBackend backend;

  BleGattBackendFixture()
    : provider(new FakeBleProvider()),
      backend("A1:B2:C3:D4:E5:F6", std::unique_ptr<ndx::BleProvider>(provider)) {}

  void start() {
    backend.start({});
  }

  void stop() {
    backend.stop();
  }

};

TEST_CASE_METHOD(BleGattBackendFixture, "BleGattBackend can be instantiated") {
  REQUIRE(true);
}

TEST_CASE_METHOD(BleGattBackendFixture, "BleGattBackend start sets is_running to true") {
  start();
  REQUIRE(backend.is_running());
}

TEST_CASE_METHOD(BleGattBackendFixture, "BleGattBackend invokes callback when packet received") {
  bool called = false;
  const std::string uuid = "test-char-uuid";
  backend.start({{uuid, std::nullopt, [&](const ndx::Packet&) { called = true; }}});
  provider->simulate_packet(ndx::Packet{}, uuid);
  REQUIRE(called);
}

TEST_CASE_METHOD(BleGattBackendFixture, "BleGattBackend only invokes callback for matching char UUID") {
  int a_calls = 0, b_calls = 0;
  backend.start({
    {"uuid-a", std::nullopt, [&](const ndx::Packet&) { a_calls++; }},
    {"uuid-b", std::nullopt, [&](const ndx::Packet&) { b_calls++; }},
  });
  provider->simulate_packet(ndx::Packet{}, "uuid-a");
  REQUIRE(a_calls == 1);
  REQUIRE(b_calls == 0);
}

TEST_CASE_METHOD(BleGattBackendFixture, "BleGattBackend does not invoke callback for unregistered char UUID") {
  bool called = false;
  backend.start({{"uuid-a", std::nullopt, [&](const ndx::Packet&) { called = true; }}});
  provider->simulate_packet(ndx::Packet{}, "uuid-b");
  REQUIRE_FALSE(called);
}

TEST_CASE_METHOD(BleGattBackendFixture, "BleGattBackend stop sets is_running to false") {
  start();
  stop();
  REQUIRE_FALSE(backend.is_running());
}

TEST_CASE_METHOD(BleGattBackendFixture, "BleGattBackend stop throws if not running") {
  REQUIRE_THROWS_WITH(stop(), "BleGattBackend: stop called while not running");
}

TEST_CASE_METHOD(BleGattBackendFixture, "BleGattBackend stop calls stop on provider") {
  start();
  stop();

  REQUIRE(provider->disconnect_requested_for == "A1:B2:C3:D4:E5:F6");
}

TEST_CASE_METHOD(BleGattBackendFixture, "BleGattBackend start throws if already running") {
  start();
  REQUIRE_THROWS_WITH(start(), "BleGattBackend: start called while already running");
}

TEST_CASE_METHOD(BleGattBackendFixture, "BleGattBackend start throws when Bluetooth is not powered on") {
  provider->powered_on = false;
  REQUIRE_THROWS_WITH(start(), "BleGattBackend: Bluetooth is not powered on");
}

TEST_CASE_METHOD(BleGattBackendFixture, "BleGattBackend start scans for peripheral with device_id") {
  start();
  REQUIRE(provider->scan_requested_for == "A1:B2:C3:D4:E5:F6");
}

TEST_CASE_METHOD(BleGattBackendFixture, "BleGattBackend reports an unexpected disconnect and stays running") {
  int disconnects = 0;
  bool reported_intentional = true;
  backend.start({}, nullptr, [&](bool intentional) {
    disconnects++;
    reported_intentional = intentional;
  });
  provider->simulate_unexpected_disconnect();
  REQUIRE(disconnects == 1);
  REQUIRE_FALSE(reported_intentional);
  REQUIRE(backend.is_running());
}

TEST_CASE_METHOD(BleGattBackendFixture, "BleGattBackend sets is_intentional_disconnect false") {
  REQUIRE_FALSE(backend.is_intentional_disconnect());
}

TEST_CASE_METHOD(BleGattBackendFixture, "BleGattBackend write_characteristic forwards data to provider") {
  const uint8_t data[] = {0x02, 'h', '\n'};
  backend.write_characteristic("273E0001-4C4D-454D-96BE-F03BAC821358", data, sizeof(data));
  REQUIRE(provider->last_write_char_uuid == "273E0001-4C4D-454D-96BE-F03BAC821358");
  REQUIRE(provider->last_write_data == std::vector<uint8_t>{0x02, 'h', '\n'});
}

TEST_CASE_METHOD(BleGattBackendFixture, "BleGattBackend add_char_callbacks forwards to provider") {
  start();
  backend.add_char_callbacks({{"added-uuid", std::nullopt, [](const ndx::Packet&) {}}});
  REQUIRE(provider->add_char_callbacks_called);
}

TEST_CASE_METHOD(BleGattBackendFixture, "BleGattBackend add_char_callbacks routes packets to the added callback") {
  start();
  bool called = false;
  backend.add_char_callbacks({{"added-uuid", std::nullopt, [&](const ndx::Packet&) { called = true; }}});
  provider->simulate_packet(ndx::Packet{}, "added-uuid");
  REQUIRE(called);
}
