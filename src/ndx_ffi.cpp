#include <cstring>
#include <string>
#include <unordered_map>
#include <memory>
#include <nlohmann/json.hpp>
#include "ndx/ndx_ffi.hpp"
#include "ndx/ble_backend.hpp"
#include "ndx/ble_provider.hpp"
#include "ndx/ftdi_backend.hpp"
#include "ndx/ndx_ffi_impl.hpp"

static std::unordered_map<int, std::shared_ptr<ndx::BleBackend>> g_ble_backends;
static std::unordered_map<int, std::shared_ptr<ndx::FtdiBackend>> g_ftdi_backends;

static int g_next_ble_id = 1;
static int g_next_ftdi_id = 1;

static BleFactory g_ble_factory = [](const std::string& uuid) {
    return std::make_shared<ndx::BleBackend>(uuid, ndx::createBleProvider());
};

static FtdiFactory g_ftdi_factory = [](const std::string& serial_number) {
    return std::make_shared<ndx::FtdiBackend>(serial_number);
};

std::shared_ptr<ndx::BleBackend> getBleBackend(int id) {
    auto it = g_ble_backends.find(id);
    return (it != g_ble_backends.end()) ? it->second : nullptr;
}

std::shared_ptr<ndx::FtdiBackend> getFtdiBackend(int id) {
    auto it = g_ftdi_backends.find(id);
    return (it != g_ftdi_backends.end()) ? it->second : nullptr;
}

static bool is_ble_registered(const std::string& uuid) {
    for (const auto& [id, backend] : g_ble_backends) {
        if (backend->device_id() == uuid) {
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
    try {
        auto j = nlohmann::json::parse(config_json, nullptr, false);

        if (j.is_discarded()) {
            return to_ffi_result({{"status", 400}, {"error", "malformed JSON"}});
        }

        std::string uuid = j["uuid"].get<std::string>();

        if (!is_valid_uuid(uuid)) {
            return to_ffi_result({{"status", 400}, {"error", "invalid uuid"}});
        }

        if (is_ble_registered(uuid)) {
            return to_ffi_result({{"status", 400}, {"error", "uuid already registered"}});
        }

        int id = g_next_ble_id++;
        g_ble_backends[id] = g_ble_factory(uuid);

        return to_ffi_result({{"status", 200}, {"id", id}});
    } catch (const std::exception& e) {
        return to_ffi_result({{"status", 500}, {"error", e.what()}});
    }
}

extern "C" char* startBleBackend(const char* id_str, void (*on_data)(const char* packet_json)) {
    try {
        return startBackend(id_str, getBleBackend, on_data);
    } catch (const std::exception& e) {
        return to_ffi_result({{"status", 500}, {"error", e.what()}});
    }
}

extern "C" char* stopBleBackend(const char* id_str)  {
    try {
        return stopBackend(id_str, getBleBackend); 
    } catch (const std::exception& e) {
        return to_ffi_result({{"status", 500}, {"error", e.what()}});
    }
}

extern "C" char* destroyBleBackend(const char* id_str) {
    try {
        char* result = destroyBackend(id_str, getBleBackend);
        int id = std::stoi(id_str);
        g_ble_backends.erase(id);
        return result;
    } catch (const std::exception& e) {
        return to_ffi_result({{"status", 500}, {"error", e.what()}});
    }
}

extern "C" char* getRssiBleBackend(const char* id_str) {
    try {
        int id = std::stoi(id_str);
        auto backend = getBleBackend(id);
        int rssi = backend->getRssi();
        return to_ffi_result({{"status", 200}, {"rssi", rssi}});
    } catch (const std::exception& e) {
        return to_ffi_result({{"status", 500}, {"error", e.what()}});
    }
}

extern "C" char* createFtdiBackend(const char* config_json) {
    try {
        auto j = nlohmann::json::parse(config_json, nullptr, false);

        if (j.is_discarded()) {
            return to_ffi_result({{"status", 400}, {"error", "malformed JSON"}});
        }

        std::string serial_number = j["serial_number"].get<std::string>();

        if (!is_valid_serial(serial_number)) {
            return to_ffi_result({{"status", 400}, {"error", "invalid serial number"}});
        }

        if (is_ftdi_registered(serial_number)) {
            return to_ffi_result({{"status", 400}, {"error", "serial number already registered"}});
        }

        int id = g_next_ftdi_id++;
        g_ftdi_backends[id] = g_ftdi_factory(serial_number);

        return to_ffi_result({{"status", 200}, {"id", id}});
    } catch (const std::exception& e) {
        return to_ffi_result({{"status", 500}, {"error", e.what()}});
    }
}

extern "C" char* startFtdiBackend(const char* id_str, void (*on_data)(const char* packet_json)) {
    try {
        return startBackend(id_str, getFtdiBackend, on_data);
    } catch (const std::exception& e) {
        return to_ffi_result({{"status", 500}, {"error", e.what()}});
    }
}

extern "C" char* stopFtdiBackend(const char* id_str) {
    try {
        return stopBackend(id_str, getFtdiBackend); 
    } catch (const std::exception& e) {
        return to_ffi_result({{"status", 500}, {"error", e.what()}});
    }
}

extern "C" char* destroyFtdiBackend(const char* id_str) {
    try {
        char* result = destroyBackend(id_str, getFtdiBackend);
        int id = std::stoi(id_str);
        g_ftdi_backends.erase(id);
        return result;
    } catch (const std::exception& e) {
        return to_ffi_result({{"status", 500}, {"error", e.what()}});
    }
}

// For tests only

void resetBleBackends() {
    g_ble_backends.clear();
    g_next_ble_id = 1;

    g_ble_factory = [](const std::string& uuid) {
        return std::make_shared<ndx::BleBackend>(uuid, ndx::createBleProvider());
    };
}

void resetFtdiBackends() {
    g_ftdi_backends.clear();
    g_next_ftdi_id = 1;

    g_ftdi_factory = [](const std::string& serial_number) {
        return std::make_shared<ndx::FtdiBackend>(serial_number);
    };
}

void setBleFactory(BleFactory factory) {
    g_ble_factory = factory;
}

void setFtdiFactory(FtdiFactory factory) {
    g_ftdi_factory = factory;
}