#include <cstring>
#include <string>
#include <unordered_map>
#include <memory>
#include <nlohmann/json.hpp>
#include "ndx/ndx_ffi.hpp"
#include "ndx/ble_backend.hpp"

static std::unordered_map<int, std::shared_ptr<ndx::BleBackend>> g_ble_backends;
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
    g_ble_backends[id] = std::make_shared<ndx::BleBackend>(address);
    return to_ffi_result({{"status", 200}, {"id", id}});
}

// For tests only
std::shared_ptr<ndx::BleBackend> getBleBackend(int id) {
    auto it = g_ble_backends.find(id);
    return (it != g_ble_backends.end()) ? it->second : nullptr;
}