#include <catch2/catch_all.hpp>
#include <functional>
#include "ndx/ftdi_backend.hpp"
#include "ndx/ftdi_provider.hpp"

struct FakeFtdiProvider : ndx::FtdiProvider {
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

  void simulate_packet(const ndx::Packet& p) {
    if (captured_on_data) captured_on_data(p);
  }
};

struct FtdiBackendFixture {
  FakeFtdiProvider* provider;
  ndx::FtdiBackend backend;

  FtdiBackendFixture()
    : provider(new FakeFtdiProvider()),
      backend("ABCD1234", std::unique_ptr<ndx::FtdiProvider>(provider)) {}

  void start() {
    backend.start([](const ndx::Packet&) {});
  }

  void stop() {
    backend.stop();
  }

};

TEST_CASE_METHOD(FtdiBackendFixture, "FtdiBackend can be instantiated") {
  REQUIRE(true);
}

TEST_CASE_METHOD(FtdiBackendFixture, "FtdiBackend start sets is_running to true") {
  start();
  REQUIRE(backend.is_running());
}

TEST_CASE_METHOD(FtdiBackendFixture, "FtdiBackend invokes callback when packet received") {
  bool called = false;
  backend.start([&](const ndx::Packet&) { called = true; });
  provider->simulate_packet(ndx::Packet{});
  REQUIRE(called);
}

TEST_CASE_METHOD(FtdiBackendFixture, "FtdiBackend start connects with device id") {
  start();
  REQUIRE(provider->connect_requested_for == "ABCD1234");
}

TEST_CASE_METHOD(FtdiBackendFixture, "FtdiBackend stop sets is_running to false") {
  start();
  stop();
  REQUIRE_FALSE(backend.is_running());
}

TEST_CASE_METHOD(FtdiBackendFixture, "FtdiBackend start throws if already running") {
  start();
  REQUIRE_THROWS_WITH(start(), "FtdiBackend: start called while already running");
}

TEST_CASE_METHOD(FtdiBackendFixture, "FtdiBackend stop throws if not running") {
  REQUIRE_THROWS_WITH(stop(), "FtdiBackend: stop called while not running");
}