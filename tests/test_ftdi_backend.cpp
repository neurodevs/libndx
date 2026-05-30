#include <catch2/catch_all.hpp>
#include <functional>
#include <unordered_map>
#include "ndx/ftdi_backend.hpp"

struct TestableFtdiBackend : ndx::FtdiBackend {
  using ndx::FtdiBackend::FtdiBackend;

  std::unordered_map<std::string, std::function<void(const ndx::Packet&)>> callbacks;

  void start(ndx::CharCallbacks cbs) override {
    ndx::FtdiBackend::start(cbs);
    for (auto& e : cbs) callbacks[e.char_uuid] = std::move(e.on_data);
  }

  void simulate_packet(const ndx::Packet& p, const std::string& char_uuid = "") {
    auto it = callbacks.find(char_uuid);
    if (it != callbacks.end()) it->second(p);
  }
};

struct FtdiBackendFixture {
  TestableFtdiBackend backend{ "ABCD1234" };

  void start() {
    backend.start({{"", std::nullopt, [](const ndx::Packet&) {}}});
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
  backend.start({{"", std::nullopt, [&](const ndx::Packet&) { called = true; }}});
  backend.simulate_packet(ndx::Packet{});
  REQUIRE(called);
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