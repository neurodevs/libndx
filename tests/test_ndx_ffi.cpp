#include <catch2/catch_all.hpp>
#include <nlohmann/json.hpp>
#include "ndx/ndx_ffi.hpp"

TEST_CASE("createBleBackend returns ok") {
    const char* config_json = "{\"deviceId\":\"test-device\"}";
    const char* result = createBleBackend(config_json);

    auto json = nlohmann::json::parse(result);
    REQUIRE(json["status"] == 200);
}