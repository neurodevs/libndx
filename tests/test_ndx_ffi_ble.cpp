#include <catch2/catch_all.hpp>
#include <nlohmann/json.hpp>
#include "ndx/ndx_ffi.hpp"

struct BleFfiFixture {
  BleFfiFixture() {
    resetBleBackends();
  }

  nlohmann::json createAndParse(const char* config_json) {
    const char* result = createBleBackend(config_json);
    return nlohmann::json::parse(result);
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
};

struct ValidBleFixture : BleFfiFixture {
  nlohmann::json json = createAndParse("{\"mac_address\":\"AA:BB:CC:DD:EE:FF\"}");
};

TEST_CASE_METHOD(ValidBleFixture, "createBleBackend returns ok") {
    REQUIRE(json["status"] == 200);
}

TEST_CASE_METHOD(ValidBleFixture, "createBleBackend returns id") {
    REQUIRE(json.contains("id"));
}

TEST_CASE_METHOD(ValidBleFixture, "createBleBackend autoincrements id") {
    auto json2 = createAndParse("{\"mac_address\":\"XX:XX:XX:XX:XX:XX\"}");
    REQUIRE(json2["id"] == (json["id"].get<int>() + 1));
}

TEST_CASE_METHOD(ValidBleFixture, "createBleBackend constructs BleBackend instance") {
    int id = json["id"].get<int>();
    auto backend = getBleBackend(id);
    REQUIRE(backend != nullptr);
}

TEST_CASE_METHOD(ValidBleFixture, "createBleBackend sets proper mac_address") {
    int id = json["id"].get<int>();
    auto backend = getBleBackend(id);
    REQUIRE(backend->device_id() == "AA:BB:CC:DD:EE:FF");
}

TEST_CASE_METHOD(ValidBleFixture, "createBleBackend returns 400 if address is already registered") {
    auto json = createAndParse("{\"mac_address\":\"AA:BB:CC:DD:EE:FF\"}");
    REQUIRE(json["status"] == 400);
    REQUIRE(json["error"] == "MAC address already registered");
}

TEST_CASE_METHOD(BleFfiFixture, "createBleBackend returns 400 if address is not size 17") {
    auto json = createAndParse("{\"mac_address\":\"not-mac-address\"}");
    REQUIRE(json["status"] == 400);
    REQUIRE(json["error"] == "invalid MAC address");
}

TEST_CASE_METHOD(BleFfiFixture, "createBleBackend returns 400 if address does not contain 5 colons") {
    auto json = createAndParse("{\"mac_address\":\"AA:BB:CC:DD:EE;FF\"}");
    REQUIRE(json["status"] == 400);
    REQUIRE(json["error"] == "invalid MAC address");
}

TEST_CASE_METHOD(BleFfiFixture, "createBleBackend returns 400 if JSON is malformed") {
    auto json = createAndParse("{");
    REQUIRE(json["status"] == 400);
    REQUIRE(json["error"] == "malformed JSON");
}

TEST_CASE_METHOD(BleFfiFixture, "createBleBackend returns 500 on unexpected throw") {
  setBleFactory([](const std::string&) -> std::shared_ptr<ndx::BleBackend> {
    throw std::runtime_error("hardware fault");
  });
  auto json = createAndParse("{\"mac_address\":\"AA:BB:CC:DD:EE:FF\"}");
  REQUIRE(json["status"] == 500);
  REQUIRE(json["error"].get<std::string>().find("hardware fault") != std::string::npos);
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

TEST_CASE_METHOD(ValidBleFixture, "destroyBleBackend calls stop on backend") {
    BleFfiFixture::start();
    BleFfiFixture::destroy();
    auto backend = getBleBackend(1);
    REQUIRE(!backend->is_running());
}