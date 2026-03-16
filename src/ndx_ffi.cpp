#include <cstring>
#include <string>
#include <unordered_map>
#include <memory>
#include <nlohmann/json.hpp>
#include "ndx/ndx_ffi.hpp"

extern "C" char* createBleBackend(const char* config_json) {
    auto j = nlohmann::json::parse(config_json, nullptr, false);
    std::string address = j["address"].get<std::string>();
    if (address.size() != 17) {
        return strdup("{\"status\": 400, \"error\": \"invalid MAC address\" }");
    }
    return strdup("{\"status\": 200 }");
}

