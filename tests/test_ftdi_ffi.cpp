#include <catch2/catch_all.hpp>
#include <nlohmann/json.hpp>
#include "ndx/ndx_ffi.hpp"

struct FtdiFfiFixture {
  std::string valid_serial;

  FtdiFfiFixture() {
    resetFtdiBackends();
    valid_serial = "ABCD1234";
  }

  nlohmann::json createAndParse(const char* config_json) {
    const char* result = createFtdiBackend(config_json);
    return nlohmann::json::parse(result);
  }

  nlohmann::json start() {
    const char* result = startFtdiBackend(valid_serial.c_str(), [](const char*) {});
    return nlohmann::json::parse(result);
  }

  nlohmann::json stop() {
    const char* result = stopFtdiBackend(valid_serial.c_str());
    return nlohmann::json::parse(result);
  }

  nlohmann::json destroy() {
    const char* result = destroyFtdiBackend(valid_serial.c_str());
    return nlohmann::json::parse(result);
  }

  void setThrowingFactory() {
    setFtdiFactory([](const std::string& uuid) -> std::shared_ptr<ndx::FtdiBackend> {
      struct ThrowingFtdiBackend : ndx::FtdiBackend {
        using ndx::FtdiBackend::FtdiBackend;
        void start(ndx::OnDataCallback) override { throw std::runtime_error("hardware fault"); }
        void stop() override { throw std::runtime_error("hardware fault"); }
        void destroy() override { throw std::runtime_error("hardware fault"); }
      };
      return std::make_shared<ThrowingFtdiBackend>(uuid);
    });
  }
};

struct ValidFtdiFixture : FtdiFfiFixture {
  nlohmann::json json = createAndParse("{\"serial_number\":\"ABCD1234\"}");
};

TEST_CASE_METHOD(ValidFtdiFixture, "createFtdiBackend returns ok") {
    REQUIRE(json["status"] == 200);
}

TEST_CASE_METHOD(ValidFtdiFixture, "createFtdiBackend constructs FtdiBackend instance") {
    auto backend = getFtdiBackend(valid_serial);
    REQUIRE(backend != nullptr);
}

TEST_CASE_METHOD(ValidFtdiFixture, "createFtdiBackend sets proper serial_number") {
    auto backend = getFtdiBackend(valid_serial);
    REQUIRE(backend->device_id() == "ABCD1234");
}

TEST_CASE_METHOD(ValidFtdiFixture, "createFtdiBackend returns 400 if serial number is already registered") {
    auto json = createAndParse("{\"serial_number\":\"ABCD1234\"}");
    REQUIRE(json["status"] == 400);
    REQUIRE(json["error"] == "serial number already registered");
}

TEST_CASE_METHOD(FtdiFfiFixture, "createFtdiBackend returns 400 if serial number is not size 8") {
    auto json = createAndParse("{\"serial_number\":\"not-size-eight\"}");
    REQUIRE(json["status"] == 400);
    REQUIRE(json["error"] == "invalid serial number");
}

TEST_CASE_METHOD(FtdiFfiFixture, "createFtdiBackend returns 400 if JSON is malformed") {
    auto json = createAndParse("{");
    REQUIRE(json["status"] == 400);
    REQUIRE(json["error"] == "malformed JSON");
}

TEST_CASE_METHOD(ValidFtdiFixture, "startFtdiBackend returns ok") {
    auto json = FtdiFfiFixture::start();
    REQUIRE(json["status"] == 200);
}

TEST_CASE_METHOD(ValidFtdiFixture, "startFtdiBackend calls start on backend") {
    FtdiFfiFixture::start();
    auto backend = getFtdiBackend(valid_serial);
    REQUIRE(backend->is_running());
}

TEST_CASE_METHOD(ValidFtdiFixture, "stopFtdiBackend returns ok") {
    FtdiFfiFixture::start();
    auto json = FtdiFfiFixture::stop();
    REQUIRE(json["status"] == 200);
}

TEST_CASE_METHOD(ValidFtdiFixture, "stopFtdiBackend calls stop on backend") {
    FtdiFfiFixture::start();
    FtdiFfiFixture::stop();
    auto backend = getFtdiBackend(valid_serial);
    REQUIRE(!backend->is_running());
}

TEST_CASE_METHOD(ValidFtdiFixture, "destroyFtdiBackend returns ok") {
    FtdiFfiFixture::destroy();
    REQUIRE(json["status"] == 200);
}

TEST_CASE_METHOD(ValidFtdiFixture, "destroyFtdiBackend calls stop on backend") {
    FtdiFfiFixture::start();
    auto backend = getFtdiBackend(valid_serial);
    FtdiFfiFixture::destroy();
    REQUIRE(!backend->is_running());
}

TEST_CASE_METHOD(ValidFtdiFixture, "destroyFtdiBackend removes backend from registry") {
    FtdiFfiFixture::destroy();
    auto backend = getFtdiBackend(valid_serial);
    REQUIRE(backend == nullptr);
}

TEST_CASE_METHOD(FtdiFfiFixture, "createFtdiBackend returns 500 on unexpected throw") {
  setFtdiFactory([](const std::string&) -> std::shared_ptr<ndx::FtdiBackend> {
    throw std::runtime_error("hardware fault");
  });
  auto json = createAndParse("{\"serial_number\":\"ABCD1234\"}");
  REQUIRE(json["status"] == 500);
  REQUIRE(json["error"].get<std::string>().find("hardware fault") != std::string::npos);
}

TEST_CASE_METHOD(FtdiFfiFixture, "startFtdiBackend returns 500 on unexpected throw") {
  setThrowingFactory();
  createAndParse("{\"serial_number\":\"ABCD1234\"}");
  auto json = start();
  REQUIRE(json["status"] == 500);
  REQUIRE(json["error"].get<std::string>().find("hardware fault") != std::string::npos);
}

TEST_CASE_METHOD(FtdiFfiFixture, "stopFtdiBackend returns 500 on unexpected throw") {
  setThrowingFactory();
  createAndParse("{\"serial_number\":\"ABCD1234\"}");
  start();
  auto json = stop();
  REQUIRE(json["status"] == 500);
  REQUIRE(json["error"].get<std::string>().find("hardware fault") != std::string::npos);
}

TEST_CASE_METHOD(FtdiFfiFixture, "destroyFtdiBackend returns 500 on unexpected throw") {
  setThrowingFactory();
  createAndParse("{\"serial_number\":\"ABCD1234\"}");
  auto json = destroy();
  REQUIRE(json["status"] == 500);
  REQUIRE(json["error"].get<std::string>().find("hardware fault") != std::string::npos);
}
