#include <catch2/catch_all.hpp>
#include <nlohmann/json.hpp>
#include "ndx/ndx_ffi.hpp"

struct AlwaysOnBleProvider : ndx::BleProvider {
  bool isPoweredOn() override { return true; }
  void scanForPeripheral(const std::string&, ndx::OnDataCallback) override {}
  void scanAll(int, ndx::ScanResultCallback) override {}
};

struct BleFfiFixture {
  BleFfiFixture() {
    resetBleBackends();
    setBleFactory([](const std::string& id) {
      return std::make_shared<ndx::BleBackend>(id, std::make_unique<AlwaysOnBleProvider>());
    });
    valid_device_id = "XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX";
  }

  nlohmann::json createAndParse(std::string device_id) {
    std::string config_json = "{\"device_id\":\"" + device_id + "\"}";
    const char* result = createBleBackend(config_json.c_str());
    return nlohmann::json::parse(result);
  }

  nlohmann::json createAndParseValid() {
    return createAndParse(valid_device_id);
  }

  nlohmann::json start() {
    const char* result = startBleBackend("1");
    return nlohmann::json::parse(result);
  }

  nlohmann::json stop() {
    const char* result = stopBleBackend("1");
    return nlohmann::json::parse(result);
  }

  nlohmann::json destroy() {
    const char* result = destroyBleBackend("1");
    return nlohmann::json::parse(result);
  }

  std::string valid_device_id;

  void setThrowingFactory() {
    setBleFactory([](const std::string& id) -> std::shared_ptr<ndx::BleBackend> {
      struct ThrowingBleBackend : ndx::BleBackend {
        using ndx::BleBackend::BleBackend;
        void start(ndx::OnDataCallback) override { throw std::runtime_error("hardware fault"); }
        void stop() override { throw std::runtime_error("hardware fault"); }
        void destroy() override { throw std::runtime_error("hardware fault"); }
      };
      return std::make_shared<ThrowingBleBackend>(id, std::make_unique<AlwaysOnBleProvider>());
    });
  }
};

struct ValidBleFixture : BleFfiFixture {
  nlohmann::json json = createAndParseValid();
};

TEST_CASE_METHOD(ValidBleFixture, "createBleBackend returns ok") {
    REQUIRE(json["status"] == 200);
}

TEST_CASE_METHOD(ValidBleFixture, "createBleBackend returns id") {
    REQUIRE(json.contains("id"));
}

TEST_CASE_METHOD(ValidBleFixture, "createBleBackend autoincrements id") {
    auto json2 = createAndParse("YYYYYYYY-YYYY-YYYY-YYYY-YYYYYYYYYYYY");
    REQUIRE(json2["id"] == (json["id"].get<int>() + 1));
}

TEST_CASE_METHOD(ValidBleFixture, "createBleBackend constructs BleBackend instance") {
    int id = json["id"].get<int>();
    auto backend = getBleBackend(id);
    REQUIRE(backend != nullptr);
}

TEST_CASE_METHOD(ValidBleFixture, "createBleBackend sets proper device_id") {
    int id = json["id"].get<int>();
    auto backend = getBleBackend(id);
    REQUIRE(backend->device_id() == "XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX");
}

TEST_CASE_METHOD(ValidBleFixture, "createBleBackend returns 400 if device_id is already registered") {
    auto json = createAndParseValid();
    REQUIRE(json["status"] == 400);
    REQUIRE(json["error"] == "device id already registered");
}

TEST_CASE_METHOD(BleFfiFixture, "createBleBackend returns 400 if device_id is not size 36") {
    auto json = createAndParse("not-a-valid-uuid");
    REQUIRE(json["status"] == 400);
    REQUIRE(json["error"] == "invalid device id");
}

TEST_CASE_METHOD(BleFfiFixture, "createBleBackend returns 400 if device_id does not contain 4 hyphens") {
    auto json = createAndParse("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
    REQUIRE(json["status"] == 400);
    REQUIRE(json["error"] == "invalid device id");
}

TEST_CASE_METHOD(BleFfiFixture, "createBleBackend returns 400 if JSON is malformed") {
    auto json = nlohmann::json::parse(createBleBackend("{"));
    REQUIRE(json["status"] == 400);
    REQUIRE(json["error"] == "malformed JSON");
}

TEST_CASE_METHOD(ValidBleFixture, "startBleBackend returns ok") {
    auto json = BleFfiFixture::start();
    REQUIRE(json["status"] == 200);
}

TEST_CASE_METHOD(ValidBleFixture, "startBleBackend returns id") {
    auto json = BleFfiFixture::start();
    REQUIRE(json.contains("id"));
}

TEST_CASE_METHOD(ValidBleFixture, "startBleBackend calls start on backend") {
    auto json = BleFfiFixture::start();
    auto backend = getBleBackend(1);
    REQUIRE(backend->is_running());
}

TEST_CASE_METHOD(ValidBleFixture, "stopBleBackend returns ok") {
    BleFfiFixture::start();
    auto json = BleFfiFixture::stop();
    REQUIRE(json["status"] == 200);
}

TEST_CASE_METHOD(ValidBleFixture, "stopBleBackend returns id") {
    BleFfiFixture::start();
    auto json = BleFfiFixture::stop();
    REQUIRE(json.contains("id"));
}

TEST_CASE_METHOD(ValidBleFixture, "stopBleBackend calls stop on backend") {
    BleFfiFixture::start();
    BleFfiFixture::stop();
    auto backend = getBleBackend(1);
    REQUIRE(!backend->is_running());
}

TEST_CASE_METHOD(ValidBleFixture, "destroyBleBackend returns ok") {
    BleFfiFixture::destroy();
    REQUIRE(json["status"] == 200);
}

TEST_CASE_METHOD(ValidBleFixture, "destroyBleBackend returns id") {
    BleFfiFixture::destroy();
    REQUIRE(json.contains("id"));
}

TEST_CASE_METHOD(ValidBleFixture, "destroyBleBackend calls stop on backend if running") {
    BleFfiFixture::start();
    auto backend = getBleBackend(1);
    BleFfiFixture::destroy();
    REQUIRE(!backend->is_running());
}

TEST_CASE_METHOD(ValidBleFixture, "destroyBleBackend removes backend from registry") {
    BleFfiFixture::destroy();
    auto backend = getBleBackend(1);
    REQUIRE(backend == nullptr);
}

TEST_CASE_METHOD(BleFfiFixture, "createBleBackend returns 500 on unexpected throw") {
  setBleFactory([](const std::string&) -> std::shared_ptr<ndx::BleBackend> {
    throw std::runtime_error("hardware fault");
  });
  auto json = createAndParse("XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX");
  REQUIRE(json["status"] == 500);
  REQUIRE(json["error"].get<std::string>().find("hardware fault") != std::string::npos);
}

TEST_CASE_METHOD(BleFfiFixture, "startBleBackend returns 500 on unexpected throw") {
  setThrowingFactory();
  createAndParse("XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX");
  auto json = BleFfiFixture::start();
  REQUIRE(json["status"] == 500);
  REQUIRE(json["error"].get<std::string>().find("hardware fault") != std::string::npos);
}

TEST_CASE_METHOD(BleFfiFixture, "stopBleBackend returns 500 on unexpected throw") {
  setThrowingFactory();
  createAndParse("XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX");
  start();
  auto json = stop();
  REQUIRE(json["status"] == 500);
  REQUIRE(json["error"].get<std::string>().find("hardware fault") != std::string::npos);
}

TEST_CASE_METHOD(BleFfiFixture, "destroyBleBackend returns 500 on unexpected throw") {
  setThrowingFactory();
  createAndParse("XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX");
  auto json = destroy();
  REQUIRE(json["status"] == 500);
  REQUIRE(json["error"].get<std::string>().find("hardware fault") != std::string::npos);
}