#include <cstring>
#include <string>
#include <unordered_map>
#include <memory>
#include <nlohmann/json.hpp>
#include "ndx/ndx_ffi.hpp"

static int g_next_ble_id = 1;

static bool is_valid_mac(const std::string& address) {
    const int num_colons = std::count(address.begin(), address.end(), ':');
    return address.size() == 17 && num_colons == 5;
}

static char* to_ffi_result(const nlohmann::json& j) {
    return strdup(j.dump().c_str());
}

extern "C" char* createBleBackend(const char* config_json) {
    auto j = nlohmann::json::parse(config_json, nullptr, false);
    std::string address = j["mac_address"].get<std::string>();

    if (!is_valid_mac(address)) {
        return to_ffi_result({{"status", 400}, {"error", "invalid MAC address"}});
    }

    int id = g_next_ble_id++;
    return to_ffi_result({{"status", 200}, {"id", id}});
}

