#include <catch2/catch_all.hpp>
#include "ndx/ftdi_backend.hpp"

struct FtdiBackendFixture {
  ndx::FtdiBackend backend{ "ABCD1234" };

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