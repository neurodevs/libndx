#include <catch2/catch_all.hpp>
#include <nlohmann/json.hpp>
#include "ndx/ndx_ffi.hpp"

struct FtdiFfiFixture {
  FtdiFfiFixture() {
    resetFtdiBackends();
  }

  nlohmann::json createAndParse(const char* config_json) {
    const char* result = createFtdiBackend(config_json);
    return nlohmann::json::parse(result);
  }

  nlohmann::json start() {
    const char* result = startFtdiBackend("1", [](const char*) {});
    return nlohmann::json::parse(result);
  }

  nlohmann::json stop() {
    const char* result = stopFtdiBackend("1");
    return nlohmann::json::parse(result);
  }

  nlohmann::json destroy() {
    const char* result = destroyFtdiBackend("1");
    return nlohmann::json::parse(result);
}

  void setThrowingFactory() {
    setFtdiFactory([](const std::string& id) -> std::shared_ptr<ndx::FtdiBackend> {
      struct ThrowingFtdiBackend : ndx::FtdiBackend {
        using ndx::FtdiBackend::FtdiBackend;
        void start(ndx::OnDataCallback) override { throw std::runtime_error("hardware fault"); }
        void stop() override { throw std::runtime_error("hardware fault"); }
        void destroy() override { throw std::runtime_error("hardware fault"); }
      };
      return std::make_shared<ThrowingFtdiBackend>(id);
    });
  }
};

struct ValidFtdiFixture : FtdiFfiFixture {
  nlohmann::json json = createAndParse("{\"serial_number\":\"ABCD1234\"}");
};

TEST_CASE_METHOD(ValidFtdiFixture, "createFtdiBackend returns ok") {
    REQUIRE(json["status"] == 200);
}

TEST_CASE_METHOD(ValidFtdiFixture, "createFtdiBackend returns id") {
    REQUIRE(json.contains("id"));
}

TEST_CASE_METHOD(ValidFtdiFixture, "createFtdiBackend autoincrements id") {
    auto json2 = createAndParse("{\"serial_number\":\"EFGH5678\"}");
    REQUIRE(json2["id"] == (json["id"].get<int>() + 1));
}

TEST_CASE_METHOD(ValidFtdiFixture, "createFtdiBackend constructs FtdiBackend instance") {
    int id = json["id"].get<int>();
    auto backend = getFtdiBackend(id);
    REQUIRE(backend != nullptr);
}

TEST_CASE_METHOD(ValidFtdiFixture, "createFtdiBackend sets proper serial_number") {
    int id = json["id"].get<int>();
    auto backend = getFtdiBackend(id);
    REQUIRE(backend->device_id() == "ABCD1234");
}

TEST_CASE_METHOD(ValidFtdiFixture, "createFtdiBackend returns 400 if serial number is already registered") {
    auto json = createAndParse("{\"serial_number\":\"ABCD1234\"}");
    REQUIRE(json["status"] == 400);
    REQUIRE(json["error"] == "Serial number already registered");
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

TEST_CASE_METHOD(ValidFtdiFixture, "startFtdiBackend returns id") {
    auto json = FtdiFfiFixture::start();
    REQUIRE(json.contains("id"));
}

TEST_CASE_METHOD(ValidFtdiFixture, "startFtdiBackend calls start on backend") {
    auto json = FtdiFfiFixture::start();
    auto backend = getFtdiBackend(1);
    REQUIRE(backend->is_running());
}

TEST_CASE_METHOD(ValidFtdiFixture, "stopFtdiBackend returns ok") {
    FtdiFfiFixture::start();
    auto json = FtdiFfiFixture::stop();
    REQUIRE(json["status"] == 200);
}

TEST_CASE_METHOD(ValidFtdiFixture, "stopFtdiBackend returns id") {
    FtdiFfiFixture::start();
    auto json = FtdiFfiFixture::stop();
    REQUIRE(json.contains("id"));
}

TEST_CASE_METHOD(ValidFtdiFixture, "stopFtdiBackend calls stop on backend") {
    FtdiFfiFixture::start();
    FtdiFfiFixture::stop();
    auto backend = getFtdiBackend(1);
    REQUIRE(!backend->is_running());
}

TEST_CASE_METHOD(ValidFtdiFixture, "destroyFtdiBackend returns ok") {
    FtdiFfiFixture::destroy();
    REQUIRE(json["status"] == 200);
}

TEST_CASE_METHOD(ValidFtdiFixture, "destroyFtdiBackend returns id") {
    FtdiFfiFixture::destroy();
    REQUIRE(json.contains("id"));
}

TEST_CASE_METHOD(ValidFtdiFixture, "destroyFtdiBackend calls stop on backend") {
    FtdiFfiFixture::start();
    auto backend = getFtdiBackend(1);
    FtdiFfiFixture::destroy();
    REQUIRE(!backend->is_running());
}


TEST_CASE_METHOD(ValidFtdiFixture, "destroyFtdiBackend removes backend from registry") {
    FtdiFfiFixture::destroy();
    auto backend = getFtdiBackend(1);
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