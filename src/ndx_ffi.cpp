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

static std::unordered_map<std::string, std::shared_ptr<ndx::BleBackend>> g_ble_backends;
static std::unordered_map<std::string, std::shared_ptr<ndx::FtdiBackend>> g_ftdi_backends;

static BleFactory g_ble_factory = [](const std::string& device_uuid) {
    return std::make_shared<ndx::BleBackend>(device_uuid, ndx::createBleProvider());
};

std::shared_ptr<ndx::BleBackend> getBleBackend(const std::string& device_uuid) {
    auto it = g_ble_backends.find(device_uuid);
    return it != g_ble_backends.end() ? it->second : nullptr;
}

static bool is_ble_registered(const std::string& device_uuid) {
    return g_ble_backends.count(device_uuid) > 0;
}

static FtdiFactory g_ftdi_factory = [](const std::string& serial_number) {
    return std::make_shared<ndx::FtdiBackend>(serial_number);
};

std::shared_ptr<ndx::FtdiBackend> getFtdiBackend(const std::string& serial_number) {
    auto it = g_ftdi_backends.find(serial_number);
    return it != g_ftdi_backends.end() ? it->second : nullptr;
}

static bool is_ftdi_registered(const std::string& serial_number) {
    return g_ftdi_backends.count(serial_number) > 0;
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

        g_ble_backends[uuid] = g_ble_factory(uuid);
        return to_ffi_result({{"status", 200}});
    } catch (const std::exception& e) {
        return to_ffi_result({{"status", 500}, {"error", e.what()}});
    }
}

extern "C" char* startBleBackend(const char* device_uuid, void (*on_data)(const char* packet_json)) {
    try {
        return startBackend(device_uuid, getBleBackend, on_data);
    } catch (const std::exception& e) {
        return to_ffi_result({{"status", 500}, {"error", e.what()}});
    }
}

extern "C" char* writeBleCharacteristic(const char* device_uuid, const char* char_uuid, const char* value) {
    return to_ffi_result({{"status", 200}});
}

extern "C" char* readBleRssi(const char* device_uuid) {
    try {
        auto backend = getBleBackend(device_uuid);
        int rssi = backend->readRssi();
        return to_ffi_result({{"status", 200}, {"rssi", rssi}});
    } catch (const std::exception& e) {
        return to_ffi_result({{"status", 500}, {"error", e.what()}});
    }
}

extern "C" char* stopBleBackend(const char* device_uuid)  {
    try {
        return stopBackend(device_uuid, getBleBackend); 
    } catch (const std::exception& e) {
        return to_ffi_result({{"status", 500}, {"error", e.what()}});
    }
}

extern "C" char* destroyBleBackend(const char* device_uuid) {
    try {
        char* result = destroyBackend(device_uuid, getBleBackend);
        g_ble_backends.erase(device_uuid);
        return result;
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

        g_ftdi_backends[serial_number] = g_ftdi_factory(serial_number);
        return to_ffi_result({{"status", 200}});
    } catch (const std::exception& e) {
        return to_ffi_result({{"status", 500}, {"error", e.what()}});
    }
}

extern "C" char* startFtdiBackend(const char* serial_number, void (*on_data)(const char* packet_json)) {
    try {
        return startBackend(serial_number, getFtdiBackend, on_data);
    } catch (const std::exception& e) {
        return to_ffi_result({{"status", 500}, {"error", e.what()}});
    }
}

extern "C" char* stopFtdiBackend(const char* serial_number) {
    try {
        return stopBackend(serial_number, getFtdiBackend); 
    } catch (const std::exception& e) {
        return to_ffi_result({{"status", 500}, {"error", e.what()}});
    }
}

extern "C" char* destroyFtdiBackend(const char* serial_number) {
    try {
        char* result = destroyBackend(serial_number, getFtdiBackend);
        g_ftdi_backends.erase(serial_number);
        return result;
    } catch (const std::exception& e) {
        return to_ffi_result({{"status", 500}, {"error", e.what()}});
    }
}

// For tests only

void resetBleBackends() {
    g_ble_backends.clear();
    g_ble_factory = [](const std::string& device_uuid) {
        return std::make_shared<ndx::BleBackend>(device_uuid, ndx::createBleProvider());
    };
}

void resetFtdiBackends() {
    g_ftdi_backends.clear();
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