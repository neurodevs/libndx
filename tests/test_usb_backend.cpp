#include <catch2/catch_all.hpp>
#include <functional>
#include "ndx/usb_backend.hpp"
#include "ndx/usb_provider.hpp"

namespace {

struct FakeUsbProvider : ndx::UsbProvider {
  std::string connect_requested_for;
  bool disconnect_called = false;
  ndx::OnDataCallback captured_on_data;

  void connect(const std::string& device_id,
               ndx::OnDataCallback on_data,
               ndx::OnConnectedCallback) override {
    connect_requested_for = device_id;
    captured_on_data = std::move(on_data);
  }

  void disconnect() override { disconnect_called = true; }

  bool write(const uint8_t*, size_t) override { return false; }

  void simulate_packet(const ndx::Packet& p) {
    if (captured_on_data) captured_on_data(p);
  }
};

}

struct UsbBackendFixture {
  FakeUsbProvider* provider;
  ndx::UsbBackend backend;

  UsbBackendFixture()
    : provider(new FakeUsbProvider()),
      backend("ABCD1234", std::unique_ptr<ndx::UsbProvider>(provider)) {}

  void start() {
    backend.start([](const ndx::Packet&) {});
  }

  void stop() {
    backend.stop();
  }

};

TEST_CASE_METHOD(UsbBackendFixture, "UsbBackend can be instantiated") {
  REQUIRE(true);
}

TEST_CASE_METHOD(UsbBackendFixture, "UsbBackend start sets is_running to true") {
  start();
  REQUIRE(backend.is_running());
}

TEST_CASE_METHOD(UsbBackendFixture, "UsbBackend invokes callback when packet received") {
  bool called = false;
  backend.start([&](const ndx::Packet&) { called = true; });
  provider->simulate_packet(ndx::Packet{});
  REQUIRE(called);
}

TEST_CASE_METHOD(UsbBackendFixture, "UsbBackend start connects with device id") {
  start();
  REQUIRE(provider->connect_requested_for == "ABCD1234");
}

TEST_CASE_METHOD(UsbBackendFixture, "UsbBackend stop sets is_running to false") {
  start();
  stop();
  REQUIRE_FALSE(backend.is_running());
}

TEST_CASE_METHOD(UsbBackendFixture, "UsbBackend start throws if already running") {
  start();
  REQUIRE_THROWS_WITH(start(), "UsbBackend: start called while already running");
}

TEST_CASE_METHOD(UsbBackendFixture, "UsbBackend stop throws if not running") {
  REQUIRE_THROWS_WITH(stop(), "UsbBackend: stop called while not running");
}
