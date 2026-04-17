#include <catch2/catch_all.hpp>
#include <nlohmann/json.hpp>
#include "ndx/ndx_ffi.hpp"

struct FtdiFfiFixture {
  std::string valid_serial;

  FtdiFfiFixture() {
    reset_ftdi_backends();
    valid_serial = "ABCD1234";
  }

  nlohmann::json create_and_parse(const char* config_json) {
    const char* result = create_ftdi_backend(config_json);
    return nlohmann::json::parse(result);
  }

  nlohmann::json start() {
    const char* result = start_ftdi_backend(valid_serial.c_str(), [](const char*) {});
    return nlohmann::json::parse(result);
  }

  nlohmann::json stop() {
    const char* result = stop_ftdi_backend(valid_serial.c_str());
    return nlohmann::json::parse(result);
  }

  nlohmann::json destroy() {
    const char* result = destroy_ftdi_backend(valid_serial.c_str());
    return nlohmann::json::parse(result);
  }

  void set_throwing_factory() {
    set_ftdi_factory([](const std::string& uuid) -> std::shared_ptr<ndx::FtdiBackend> {
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
  nlohmann::json json = create_and_parse("{\"serial_number\":\"ABCD1234\"}");
};

TEST_CASE_METHOD(ValidFtdiFixture, "create_ftdi_backend returns ok") {
    REQUIRE(json["status"] == 200);
}

TEST_CASE_METHOD(ValidFtdiFixture, "create_ftdi_backend constructs FtdiBackend instance") {
    auto backend = get_ftdi_backend(valid_serial);
    REQUIRE(backend != nullptr);
}

TEST_CASE_METHOD(ValidFtdiFixture, "create_ftdi_backend sets proper serial_number") {
    auto backend = get_ftdi_backend(valid_serial);
    REQUIRE(backend->device_id() == "ABCD1234");
}

TEST_CASE_METHOD(ValidFtdiFixture, "create_ftdi_backend returns 400 if serial number is already registered") {
    auto json = create_and_parse("{\"serial_number\":\"ABCD1234\"}");
    REQUIRE(json["status"] == 400);
    REQUIRE(json["error"] == "serial number already registered");
}

TEST_CASE_METHOD(FtdiFfiFixture, "create_ftdi_backend returns 400 if serial number is not size 8") {
    auto json = create_and_parse("{\"serial_number\":\"not-size-eight\"}");
    REQUIRE(json["status"] == 400);
    REQUIRE(json["error"] == "invalid serial number");
}

TEST_CASE_METHOD(FtdiFfiFixture, "create_ftdi_backend returns 400 if JSON is malformed") {
    auto json = create_and_parse("{");
    REQUIRE(json["status"] == 400);
    REQUIRE(json["error"] == "malformed JSON");
}

TEST_CASE_METHOD(ValidFtdiFixture, "start_ftdi_backend returns ok") {
    auto json = FtdiFfiFixture::start();
    REQUIRE(json["status"] == 200);
}

TEST_CASE_METHOD(ValidFtdiFixture, "start_ftdi_backend calls start on backend") {
    FtdiFfiFixture::start();
    auto backend = get_ftdi_backend(valid_serial);
    REQUIRE(backend->is_running());
}

TEST_CASE_METHOD(ValidFtdiFixture, "stop_ftdi_backend returns ok") {
    FtdiFfiFixture::start();
    auto json = FtdiFfiFixture::stop();
    REQUIRE(json["status"] == 200);
}

TEST_CASE_METHOD(ValidFtdiFixture, "stop_ftdi_backend calls stop on backend") {
    FtdiFfiFixture::start();
    FtdiFfiFixture::stop();
    auto backend = get_ftdi_backend(valid_serial);
    REQUIRE(!backend->is_running());
}

TEST_CASE_METHOD(ValidFtdiFixture, "destroy_ftdi_backend returns ok") {
    FtdiFfiFixture::destroy();
    REQUIRE(json["status"] == 200);
}

TEST_CASE_METHOD(ValidFtdiFixture, "destroy_ftdi_backend calls stop on backend") {
    FtdiFfiFixture::start();
    auto backend = get_ftdi_backend(valid_serial);
    FtdiFfiFixture::destroy();
    REQUIRE(!backend->is_running());
}

TEST_CASE_METHOD(ValidFtdiFixture, "destroy_ftdi_backend removes backend from registry") {
    FtdiFfiFixture::destroy();
    auto backend = get_ftdi_backend(valid_serial);
    REQUIRE(backend == nullptr);
}

TEST_CASE_METHOD(FtdiFfiFixture, "create_ftdi_backend returns 500 on unexpected throw") {
  set_ftdi_factory([](const std::string&) -> std::shared_ptr<ndx::FtdiBackend> {
    throw std::runtime_error("hardware fault");
  });
  auto json = create_and_parse("{\"serial_number\":\"ABCD1234\"}");
  REQUIRE(json["status"] == 500);
  REQUIRE(json["error"].get<std::string>().find("hardware fault") != std::string::npos);
}

TEST_CASE_METHOD(FtdiFfiFixture, "start_ftdi_backend returns 500 on unexpected throw") {
  set_throwing_factory();
  create_and_parse("{\"serial_number\":\"ABCD1234\"}");
  auto json = start();
  REQUIRE(json["status"] == 500);
  REQUIRE(json["error"].get<std::string>().find("hardware fault") != std::string::npos);
}

TEST_CASE_METHOD(FtdiFfiFixture, "stop_ftdi_backend returns 500 on unexpected throw") {
  set_throwing_factory();
  create_and_parse("{\"serial_number\":\"ABCD1234\"}");
  start();
  auto json = stop();
  REQUIRE(json["status"] == 500);
  REQUIRE(json["error"].get<std::string>().find("hardware fault") != std::string::npos);
}

TEST_CASE_METHOD(FtdiFfiFixture, "destroy_ftdi_backend returns 500 on unexpected throw") {
  set_throwing_factory();
  create_and_parse("{\"serial_number\":\"ABCD1234\"}");
  auto json = destroy();
  REQUIRE(json["status"] == 500);
  REQUIRE(json["error"].get<std::string>().find("hardware fault") != std::string::npos);
}
