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

TEST_CASE_METHOD(ValidFtdiFixture, "createFtdiBackend returns error if serial number is already registered") {
    auto json = createAndParse("{\"serial_number\":\"ABCD1234\"}");
    REQUIRE(json["status"] == 400);
    REQUIRE(json["error"] == "Serial number already registered");
}

TEST_CASE_METHOD(ValidFtdiFixture, "createFtdiBackend sets proper serial_number") {
    int id = json["id"].get<int>();
    auto backend = getFtdiBackend(id);
    REQUIRE(backend->device_id() == "ABCD1234");
}

TEST_CASE_METHOD(FtdiFfiFixture, "createFtdiBackend returns error if serial number is not size 8") {
    auto json = createAndParse("{\"serial_number\":\"not-size-eight\"}");
    REQUIRE(json["status"] == 400);
    REQUIRE(json["error"] == "invalid serial number");
}

TEST_CASE_METHOD(ValidFtdiFixture, "startFtdiBackend returns ok") {
    const char* result = startFtdiBackend("ABCD1234");
    auto json = nlohmann::json::parse(result);
    REQUIRE(json["status"] == 200);
}