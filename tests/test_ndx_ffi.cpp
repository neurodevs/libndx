#include <catch2/catch_all.hpp>
#include <nlohmann/json.hpp>
#include "ndx/ndx_ffi.hpp"

TEST_CASE("createBleBackend returns ok") {
    const char* config_json = "{\"address\":\"AA:BB:CC:DD:EE:FF\"}";
    const char* result = createBleBackend(config_json);

    auto json = nlohmann::json::parse(result);
    REQUIRE(json["status"] == 200);
}

TEST_CASE("createBleBackend returns error if address is not size 17") {
    const char* config_json = "{\"address\":\"not-mac-address\"}";
    const char* result = createBleBackend(config_json);

    auto json = nlohmann::json::parse(result);
    REQUIRE(json["status"] == 400);
    REQUIRE(json["error"] == "invalid MAC address");
}
