#include <catch2/catch_all.hpp>
#include <functional>
#include "ndx/acquisition_backend.hpp"
#include "ndx/ble_observer_backend.hpp"

struct FakeAdvertisementProvider : ndx::BleProvider {
  bool powered_on = true;
  std::string listen_requested_for;
  bool stop_listening_called = false;
  ndx::OnDataCallback on_data;

  bool is_powered_on() override { return powered_on; }

  void start_advertisement_listen(const std::string& uuid, ndx::OnDataCallback on_data_cb) override {
    listen_requested_for = uuid;
    on_data = std::move(on_data_cb);
  }

  void stop_advertisement_listen() override {
    stop_listening_called = true;
  }

  void simulate_advertisement(const ndx::Packet& p) {
    if (on_data) on_data(p);
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
  FakeAdvertisementProvider* provider;
  ndx::BleObserverBackend backend;

  BleObserverBackendFixture()
    : provider(new FakeAdvertisementProvider()),
      backend("179F4A82-A2DF-C241-DB2A-1DF990779106",
              std::unique_ptr<ndx::BleProvider>(provider)) {}

  void start() {
    backend.start([](const ndx::Packet&) {});
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
