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

  const char* start() {
    return startBleBackend("AA:BB:CC:DD:EE:FF");
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

TEST_CASE_METHOD(ValidBleFixture, "createBleBackend returns error if address is already registered") {
    auto json = createAndParse("{\"mac_address\":\"AA:BB:CC:DD:EE:FF\"}");
    REQUIRE(json["status"] == 400);
    REQUIRE(json["error"] == "MAC address already registered");
}

TEST_CASE_METHOD(BleFfiFixture, "createBleBackend returns error if address is not size 17") {
    auto json = createAndParse("{\"mac_address\":\"not-mac-address\"}");
    REQUIRE(json["status"] == 400);
    REQUIRE(json["error"] == "invalid MAC address");
}

TEST_CASE_METHOD(BleFfiFixture, "createBleBackend returns error if address does not contain 5 colons") {
    auto json = createAndParse("{\"mac_address\":\"AA:BB:CC:DD:EE;FF\"}");
    REQUIRE(json["status"] == 400);
    REQUIRE(json["error"] == "invalid MAC address");
}

TEST_CASE_METHOD(ValidBleFixture, "startBleBackend returns ok") {
    BleFfiFixture::start();
    REQUIRE(json["status"] == 200);
}

TEST_CASE_METHOD(ValidBleFixture, "startBleBackend returns id") {
    const char* result = BleFfiFixture::start();
    auto json = nlohmann::json::parse(result);
    REQUIRE(json.contains("id"));
}

