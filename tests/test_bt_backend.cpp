#include <catch2/catch_all.hpp>
#include <functional>
#include "ndx/acquisition_backend.hpp"
#include "ndx/bt_backend.hpp"

struct FakeBtProvider : ndx::BtProvider {
  bool powered_on = true;
  std::string connect_requested_for;
  int connect_requested_port = -1;
  bool disconnect_called = false;
  ndx::OnDataCallback captured_on_data;

  bool is_powered_on() override { return powered_on; }

  void connect(const std::string& address, int port,
               ndx::OnDataCallback on_data,
               ndx::OnConnectedCallback) override {
    connect_requested_for = address;
    connect_requested_port = port;
    captured_on_data = std::move(on_data);
  }

  void disconnect() override { disconnect_called = true; }

  void simulate_packet(const ndx::Packet& p) {
    if (captured_on_data) captured_on_data(p);
  }
};

struct BtBackendFixture {
  FakeBtProvider* provider;
  ndx::BtBackend backend;

  BtBackendFixture()
    : provider(new FakeBtProvider()),
      backend("AA:BB:CC:DD:EE:FF", 1, std::unique_ptr<ndx::BtProvider>(provider)) {}

  void start(ndx::OnDataCallback on_data = nullptr) {
    backend.start(std::move(on_data));
  }

  void stop() { backend.stop(); }
};

TEST_CASE_METHOD(BtBackendFixture, "BtBackend can be instantiated") {
  REQUIRE(true);
}

TEST_CASE_METHOD(BtBackendFixture, "BtBackend start sets is_running to true") {
  start();
  REQUIRE(backend.is_running());
}

TEST_CASE_METHOD(BtBackendFixture, "BtBackend stop sets is_running to false") {
  start();
  stop();
  REQUIRE_FALSE(backend.is_running());
}

TEST_CASE_METHOD(BtBackendFixture, "BtBackend start throws if already running") {
  start();
  REQUIRE_THROWS_WITH(start(), "BtBackend: start called while already running");
}

TEST_CASE_METHOD(BtBackendFixture, "BtBackend stop throws if not running") {
  REQUIRE_THROWS_WITH(stop(), "BtBackend: stop called while not running");
}

TEST_CASE_METHOD(BtBackendFixture, "BtBackend start throws when Bluetooth is not powered on") {
  provider->powered_on = false;
  REQUIRE_THROWS_WITH(start(), "BtBackend: Bluetooth is not powered on");
}

TEST_CASE_METHOD(BtBackendFixture, "BtBackend start connects with device address and port") {
  start();
  REQUIRE(provider->connect_requested_for == "AA:BB:CC:DD:EE:FF");
  REQUIRE(provider->connect_requested_port == 1);
}

TEST_CASE_METHOD(BtBackendFixture, "BtBackend stop calls disconnect on provider") {
  start();
  stop();
  REQUIRE(provider->disconnect_called);
}

TEST_CASE_METHOD(BtBackendFixture, "BtBackend invokes callback when packet received") {
  bool called = false;
  start([&](const ndx::Packet&) { called = true; });
  provider->simulate_packet(ndx::Packet{});
  REQUIRE(called);
}

