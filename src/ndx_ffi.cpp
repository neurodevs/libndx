#include <cstring>
#include <string>
#include <unordered_map>
#include <memory>
#include <nlohmann/json.hpp>
#include "ndx/ndx_ffi.hpp"
#include "ndx/ble_backend.hpp"
#include "ndx/ftdi_backend.hpp"
#include "ndx/ndx_ffi_impl.hpp"

static std::unordered_map<int, std::shared_ptr<ndx::BleBackend>> g_ble_backends;
static std::unordered_map<int, std::shared_ptr<ndx::FtdiBackend>> g_ftdi_backends;

static int g_next_ble_id = 1;
static int g_next_ftdi_id = 1;

std::shared_ptr<ndx::BleBackend> getBleBackend(int id) {
    auto it = g_ble_backends.find(id);
    return (it != g_ble_backends.end()) ? it->second : nullptr;
}

std::shared_ptr<ndx::FtdiBackend> getFtdiBackend(int id) {
    auto it = g_ftdi_backends.find(id);
    return (it != g_ftdi_backends.end()) ? it->second : nullptr;
}

static bool is_ble_registered(const std::string& address) {
    for (const auto& [id, backend] : g_ble_backends) {
        if (backend->device_id() == address) {
            return true;
        }
    }
    return false;
}

static bool is_ftdi_registered(const std::string& serial_number) {
    for (const auto& [id, backend] : g_ftdi_backends) {
        if (backend->device_id() == serial_number) {
            return true;
        }
    }
    return false;
}

extern "C" char* createBleBackend(const char* config_json) {
    auto j = nlohmann::json::parse(config_json, nullptr, false);

    if (j.is_discarded()) {
        return to_ffi_result({{"status", 400}, {"error", "malformed JSON"}});
    }

    std::string address = j["mac_address"].get<std::string>();

    if (!is_valid_mac(address)) {
        return to_ffi_result({{"status", 400}, {"error", "invalid MAC address"}});
    }

    if (is_ble_registered(address)) {
        return to_ffi_result({{"status", 400}, {"error", "MAC address already registered"}});
    }

    int id = g_next_ble_id++;
    g_ble_backends[id] = std::make_shared<ndx::BleBackend>(address);
    return to_ffi_result({{"status", 200}, {"id", id}});
}

extern "C" char* startBleBackend(const char* id_str) {
    return startBackend(id_str, getBleBackend); 
}

extern "C" char* stopBleBackend(const char* id_str)  {
    return stopBackend(id_str, getBleBackend); 
}

extern "C" char* destroyBleBackend(const char* id_str) {
    return destroyBackend(id_str, getBleBackend);
}

extern "C" char* createFtdiBackend(const char* config_json) {
    auto j = nlohmann::json::parse(config_json, nullptr, false);

    if (j.is_discarded()) {
        return to_ffi_result({{"status", 400}, {"error", "malformed JSON"}});
    }

    std::string serial_number = j["serial_number"].get<std::string>();

    if (!is_valid_serial(serial_number)) {
        return to_ffi_result({{"status", 400}, {"error", "invalid serial number"}});
    }

    if (is_ftdi_registered(serial_number)) {
        return to_ffi_result({{"status", 400}, {"error", "Serial number already registered"}});
    }

    int id = g_next_ftdi_id++;
    g_ftdi_backends[id] = std::make_shared<ndx::FtdiBackend>(serial_number);
    return to_ffi_result({{"status", 200}, {"id", id}});
}

extern "C" char* startFtdiBackend(const char* id_str) {
    return startBackend(id_str, getFtdiBackend); 
}

extern "C" char* stopFtdiBackend(const char* id_str) {
    return stopBackend(id_str, getFtdiBackend); 
}

extern "C" char* destroyFtdiBackend(const char* id_str) {
    return destroyBackend(id_str, getFtdiBackend);
}

// For tests only

void resetBleBackends() {
    g_ble_backends.clear();
    g_next_ble_id = 1;
}

void resetFtdiBackends() {
    g_ftdi_backends.clear();
    g_next_ftdi_id = 1;
}