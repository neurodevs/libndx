#include <catch2/catch_all.hpp>
#include <functional>
#include <vector>
#include "ndx/rfcomm_backend.hpp"

struct FakeRfcommProvider : ndx::RfcommProvider {
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

  std::vector<uint8_t> written_data;
  bool write(const uint8_t* data, size_t len) override {
    written_data.assign(data, data + len);
    return true;
  }

  void simulate_packet(const ndx::Packet& p) {
    if (captured_on_data) captured_on_data(p);
  }
};

struct RfcommBackendFixture {
  FakeRfcommProvider* provider;
  ndx::RfcommBackend backend;

  RfcommBackendFixture()
    : provider(new FakeRfcommProvider()),
      backend("AA:BB:CC:DD:EE:FF", 1, std::unique_ptr<ndx::RfcommProvider>(provider)) {}

  void start(ndx::OnDataCallback on_data = nullptr) {
    backend.start(std::move(on_data));
  }

  void stop() { backend.stop(); }
};

TEST_CASE_METHOD(RfcommBackendFixture, "RfcommBackend can be instantiated") {
  REQUIRE(true);
}

TEST_CASE_METHOD(RfcommBackendFixture, "RfcommBackend start sets is_running to true") {
  start();
  REQUIRE(backend.is_running());
}

TEST_CASE_METHOD(RfcommBackendFixture, "RfcommBackend stop sets is_running to false") {
  start();
  stop();
  REQUIRE_FALSE(backend.is_running());
}

TEST_CASE_METHOD(RfcommBackendFixture, "RfcommBackend start throws if already running") {
  start();
  REQUIRE_THROWS_WITH(start(), "RfcommBackend: start called while already running");
}

TEST_CASE_METHOD(RfcommBackendFixture, "RfcommBackend stop throws if not running") {
  REQUIRE_THROWS_WITH(stop(), "RfcommBackend: stop called while not running");
}

TEST_CASE_METHOD(RfcommBackendFixture, "RfcommBackend start throws when Bluetooth is not powered on") {
  provider->powered_on = false;
  REQUIRE_THROWS_WITH(start(), "RfcommBackend: Bluetooth is not powered on");
}

TEST_CASE_METHOD(RfcommBackendFixture, "RfcommBackend start connects with device address and port") {
  start();
  REQUIRE(provider->connect_requested_for == "AA:BB:CC:DD:EE:FF");
  REQUIRE(provider->connect_requested_port == 1);
}

TEST_CASE_METHOD(RfcommBackendFixture, "RfcommBackend stop calls disconnect on provider") {
  start();
  stop();
  REQUIRE(provider->disconnect_called);
}

TEST_CASE_METHOD(RfcommBackendFixture, "RfcommBackend invokes callback when packet received") {
  bool called = false;
  start([&](const ndx::Packet&) { called = true; });
  provider->simulate_packet(ndx::Packet{});
  REQUIRE(called);
}

TEST_CASE_METHOD(RfcommBackendFixture, "RfcommBackend write forwards bytes to provider") {
  start();
  const uint8_t bytes[] = {0x02, 0x23, 0x00, 0x00, 0x03};
  REQUIRE(backend.write(bytes, sizeof(bytes)));
  REQUIRE(provider->written_data == std::vector<uint8_t>{0x02, 0x23, 0x00, 0x00, 0x03});
}
