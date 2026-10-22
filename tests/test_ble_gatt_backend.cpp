#include <catch2/catch_all.hpp>
#include <functional>
#include <unordered_map>
#include "ndx/acquisition_backend.hpp"
#include "ndx/ble_gatt_backend.hpp"

struct FakeBleProvider : ndx::BleProvider {
  void start_ble_scan(const std::string&, std::function<void(const std::string&, const std::string&)>) override {}
  void stop_ble_scan() override {}
  bool powered_on = true;
  std::string scan_requested_for;
  std::string disconnect_requested_for;

  std::string last_write_char_uuid;
  std::vector<uint8_t> last_write_data;

  std::unordered_map<std::string, std::function<void(const ndx::Packet&)>> callbacks;

  bool is_powered_on() override { return powered_on; }
  
  int read_rssi() override { return 0; }

  int rssi_interval_ms = 0;
  int num_rssi_intervals_set = 0;

  void set_rssi_interval(int interval_ms, std::function<void(int)>) override {
    rssi_interval_ms = interval_ms;
    num_rssi_intervals_set++;
  }

  void stop_rssi_interval() override { rssi_interval_ms = 0; }

  void write_characteristic(const std::string& char_uuid, const uint8_t* data, size_t len) override {
    last_write_char_uuid = char_uuid;
    last_write_data.assign(data, data + len);
  }

  ndx::OnConnectedCallback on_connected;
  ndx::OnPeripheralDisconnectedCallback on_disconnected;

  void scan_for_peripheral(const std::string& uuid, ndx::CharCallbacks cbs,  ndx::OnConnectedCallback on_connected_cb,
                           ndx::OnPeripheralDisconnectedCallback on_disconnected_cb) override {
    scan_requested_for = uuid;
    on_connected = std::move(on_connected_cb);
    on_disconnected = std::move(on_disconnected_cb);
    for (auto& e : cbs) callbacks[e.char_uuid] = std::move(e.on_data);
  }

  void simulate_connected() {
    ndx::Device device{.id = "A1:B2:C3:D4:E5:F6", .name = "Muse-1234"};
    if (on_connected) on_connected(&device);
  }

  void simulate_unexpected_disconnect() {
    if (on_disconnected) on_disconnected(false);
  }

  void simulate_intentional_disconnect() {
    if (on_disconnected) on_disconnected(true);
  }

  bool add_char_callbacks_called = false;

  void add_char_callbacks(ndx::CharCallbacks cbs) override {
    add_char_callbacks_called = true;
    for (auto& e : cbs) callbacks[e.char_uuid] = std::move(e.on_data);
  }

  void discover_ble_uuid(const std::string&, std::function<void(const std::string&)>) override {}

  void start_advertisement_listen(const std::string&, ndx::OnAdvertisementCallback) override {}
  void stop_advertisement_listen() override {}

  
  std::string reconnect_requested_for;

  void reconnect_peripheral(const std::string& uuid) override {
    reconnect_requested_for = uuid;
  }

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

  int num_connects = 0;
  std::vector<std::string> events;

  void start_reporting_connections() {
    backend.start({}, [&](const ndx::Device*) { num_connects++; },
                  [&] { events.push_back("lost"); }, [&] { events.push_back("reconnect"); },
                  [&] { events.push_back("disconnected"); });
    provider->simulate_connected();
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

TEST_CASE_METHOD(BleGattBackendFixture, "BleGattBackend reports a lost connection and stays running") {
  start_reporting_connections();
  provider->simulate_unexpected_disconnect();
  REQUIRE(events == std::vector<std::string>{"lost"});
  REQUIRE(backend.is_running());
}

TEST_CASE_METHOD(BleGattBackendFixture, "BleGattBackend reconnects once the connection is lost") {
  start_reporting_connections();
  provider->simulate_unexpected_disconnect();
  REQUIRE(provider->reconnect_requested_for == "A1:B2:C3:D4:E5:F6");
}

TEST_CASE_METHOD(BleGattBackendFixture, "BleGattBackend reports reconnect instead of connected once it reconnects") {
  start_reporting_connections();
  provider->simulate_unexpected_disconnect();
  provider->simulate_connected();
  REQUIRE(events == std::vector<std::string>{"lost", "reconnect"});
  REQUIRE(num_connects == 1);
}

TEST_CASE_METHOD(BleGattBackendFixture, "BleGattBackend polls rssi again once it reconnects") {
  start_reporting_connections();
  backend.set_rssi_interval(5000, [](int) {});
  provider->simulate_unexpected_disconnect();
  provider->stop_rssi_interval();
  provider->simulate_connected();
  REQUIRE(provider->rssi_interval_ms == 5000);
  REQUIRE(provider->num_rssi_intervals_set == 2);
}

TEST_CASE_METHOD(BleGattBackendFixture, "BleGattBackend reports disconnected once an intentional disconnect is confirmed") {
  start_reporting_connections();
  stop();
  provider->simulate_intentional_disconnect();
  REQUIRE(events == std::vector<std::string>{"disconnected"});
  REQUIRE(provider->reconnect_requested_for.empty());
}

TEST_CASE_METHOD(BleGattBackendFixture, "BleGattBackend does not report disconnected before the disconnect is confirmed") {
  start_reporting_connections();
  stop();
  REQUIRE(events.empty());
}

TEST_CASE_METHOD(BleGattBackendFixture, "BleGattBackend neither reports nor reconnects once stopped") {
  start_reporting_connections();
  stop();
  provider->simulate_unexpected_disconnect();
  REQUIRE(events.empty());
  REQUIRE(provider->reconnect_requested_for.empty());
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
