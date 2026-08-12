#include <catch2/catch_all.hpp>
#include <functional>
#include "ndx/acquisition_backend.hpp"
#include "ndx/ble_observer_backend.hpp"

struct FakeObserverProvider : ndx::BleProvider {
  bool powered_on = true;
  std::string listen_requested_for;
  bool stop_listening_called = false;
  ndx::OnAdvertisementCallback on_advertisement;

  bool is_powered_on() override { return powered_on; }

  void start_advertisement_listen(const std::string& uuid, ndx::OnAdvertisementCallback on_advertisement_cb) override {
    listen_requested_for = uuid;
    on_advertisement = std::move(on_advertisement_cb);
  }

  void stop_advertisement_listen() override {
    stop_listening_called = true;
  }

  void simulate_advertisement(const ndx::Advertisement& a) {
    if (on_advertisement) on_advertisement(a);
  }

  void scan_for_peripheral(const std::string&, ndx::CharCallbacks, ndx::OnConnectedCallback,
                           ndx::OnDisconnectedCallback) override {}
  void add_char_callbacks(ndx::CharCallbacks) override {}
  void discover_ble_uuid(const std::string&, std::function<void(const std::string&)>) override {}
  int read_rssi() override { return 0; }
  void set_rssi_interval(int, std::function<void(int)>) override {}
  void stop_rssi_interval() override {}
  void write_characteristic(const std::string&, const uint8_t*, size_t) override {}
  void disconnect_peripheral(const std::string&) override {}
};

struct BleObserverBackendFixture {
  FakeObserverProvider* provider;
  ndx::BleObserverBackend backend;
  ndx::Advertisement received;
  int receive_count = 0;

  BleObserverBackendFixture()
    : provider(new FakeObserverProvider()),
      backend("179F4A82-A2DF-C241-DB2A-1DF990779106",
              std::unique_ptr<ndx::BleProvider>(provider)) {}

  void start() {
    backend.start([](const ndx::Advertisement&) {});
  }

  void start_capturing() {
    backend.start([this](const ndx::Advertisement& a) {
      received = a;
      receive_count += 1;
    });
  }

  ndx::Advertisement fake_advertisement() {
    ndx::Advertisement a;
    a.local_name = "TestSensor_0A1B";
    a.company_id = 0xFFFF;
    a.manufacturer_data = {0xFF, 0xFF, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    a.service_uuids = {"0000180a-0000-1000-8000-00805f9b34fb"};
    a.service_data = {{"0000180a-0000-1000-8000-00805f9b34fb", {0x01, 0x00}}};
    a.rssi = -55;
    a.tx_power_level = 4;
    a.is_connectable = true;
    a.timestamp_sec = 1.0;
    return a;
  }
};

TEST_CASE_METHOD(BleObserverBackendFixture, "BleObserverBackend start listens for advertisements with device_id") {
  start();
  REQUIRE(provider->listen_requested_for == "179F4A82-A2DF-C241-DB2A-1DF990779106");
}

TEST_CASE_METHOD(BleObserverBackendFixture, "BleObserverBackend start throws when Bluetooth is not powered on") {
  provider->powered_on = false;
  REQUIRE_THROWS_WITH(start(), "BleObserverBackend: Bluetooth is not powered on");
}

TEST_CASE_METHOD(BleObserverBackendFixture, "BleObserverBackend stop stops listening on provider") {
  start();
  backend.stop();
  REQUIRE(provider->stop_listening_called);
}

TEST_CASE_METHOD(BleObserverBackendFixture, "BleObserverBackend forwards every advertisement field unchanged") {
  start_capturing();
  auto expected = fake_advertisement();

  provider->simulate_advertisement(expected);

  REQUIRE(receive_count == 1);
  REQUIRE(received.local_name == expected.local_name);
  REQUIRE(received.company_id == expected.company_id);
  REQUIRE(received.manufacturer_data == expected.manufacturer_data);
  REQUIRE(received.service_uuids == expected.service_uuids);
  REQUIRE(received.service_data.size() == expected.service_data.size());
  for (size_t i = 0; i < expected.service_data.size(); ++i) {
    REQUIRE(received.service_data[i].uuid == expected.service_data[i].uuid);
    REQUIRE(received.service_data[i].data == expected.service_data[i].data);
  }
  REQUIRE(received.rssi == expected.rssi);
  REQUIRE(received.tx_power_level == expected.tx_power_level);
  REQUIRE(received.is_connectable == expected.is_connectable);
  REQUIRE(received.timestamp_sec == Catch::Approx(expected.timestamp_sec));
}

TEST_CASE_METHOD(BleObserverBackendFixture, "BleObserverBackend forwards advertisements missing optional fields") {
  start_capturing();

  ndx::Advertisement sparse;
  sparse.manufacturer_data = {0xFF, 0xFF};
  sparse.timestamp_sec = 2.0;
  provider->simulate_advertisement(sparse);

  REQUIRE(receive_count == 1);
  REQUIRE(received.local_name.empty());
  REQUIRE_FALSE(received.rssi.has_value());
  REQUIRE_FALSE(received.tx_power_level.has_value());
  REQUIRE(received.service_uuids.empty());
  REQUIRE(received.service_data.empty());
  REQUIRE_FALSE(received.is_connectable);
}

TEST_CASE_METHOD(BleObserverBackendFixture, "BleObserverBackend forwards advertisements with no manufacturer data") {
  start_capturing();

  ndx::Advertisement name_only;
  name_only.local_name = "TestSensor_0A1B";
  name_only.timestamp_sec = 3.0;
  provider->simulate_advertisement(name_only);

  REQUIRE(receive_count == 1);
  REQUIRE(received.local_name == "TestSensor_0A1B");
  REQUIRE(received.manufacturer_data.empty());
}
