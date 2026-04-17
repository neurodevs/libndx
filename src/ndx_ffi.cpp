#include <cstring>
#include <string>
#include <unordered_map>
#include <memory>
#include <nlohmann/json.hpp>

#include "ndx/ndx_ffi.hpp"
#include "ndx/ndx_ffi_impl.hpp"
#include "ndx/ble_backend.hpp"
#include "ndx/ble_provider.hpp"
#include "ndx/ftdi_backend.hpp"

static std::unordered_map<std::string, std::shared_ptr<ndx::BleBackend>> g_ble_backends;
static std::unordered_map<std::string, std::shared_ptr<ndx::FtdiBackend>> g_ftdi_backends;

static BleFactory g_ble_factory = [](const std::string& device_uuid) {
    return std::make_shared<ndx::BleBackend>(device_uuid, ndx::create_ble_provider());
};

std::shared_ptr<ndx::BleBackend> get_ble_backend(const std::string& device_uuid) {
    auto it = g_ble_backends.find(device_uuid);
    return it != g_ble_backends.end() ? it->second : nullptr;
}

static bool is_ble_registered(const std::string& device_uuid) {
    return g_ble_backends.count(device_uuid) > 0;
}

static FtdiFactory g_ftdi_factory = [](const std::string& serial_number) {
    return std::make_shared<ndx::FtdiBackend>(serial_number);
};

std::shared_ptr<ndx::FtdiBackend> get_ftdi_backend(const std::string& serial_number) {
    auto it = g_ftdi_backends.find(serial_number);
    return it != g_ftdi_backends.end() ? it->second : nullptr;
}

static bool is_ftdi_registered(const std::string& serial_number) {
    return g_ftdi_backends.count(serial_number) > 0;
}

extern "C" char* create_ble_backend(const char* config_json) {
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

extern "C" char* start_ble_backend(const char* device_uuid, void (*on_data)(const char* packet_json)) {
    try {
        return start_backend(device_uuid, get_ble_backend, on_data);
    } catch (const std::exception& e) {
        return to_ffi_result({{"status", 500}, {"error", e.what()}});
    }
}

extern "C" char* write_ble_characteristic(const char* device_uuid, const char* char_uuid, const char* value) {
    return to_ffi_result({{"status", 200}});
}

extern "C" char* read_ble_rssi(const char* device_uuid) {
    try {
        auto backend = get_ble_backend(device_uuid);
        int rssi = backend->read_rssi();
        return to_ffi_result({{"status", 200}, {"rssi", rssi}});
    } catch (const std::exception& e) {
        return to_ffi_result({{"status", 500}, {"error", e.what()}});
    }
}

extern "C" char* stop_ble_backend(const char* device_uuid)  {
    try {
        return stop_backend(device_uuid, get_ble_backend); 
    } catch (const std::exception& e) {
        return to_ffi_result({{"status", 500}, {"error", e.what()}});
    }
}

extern "C" char* destroy_ble_backend(const char* device_uuid) {
    try {
        char* result = destroy_backend(device_uuid, get_ble_backend);
        g_ble_backends.erase(device_uuid);
        return result;
    } catch (const std::exception& e) {
        return to_ffi_result({{"status", 500}, {"error", e.what()}});
    }
}

extern "C" char* create_ftdi_backend(const char* config_json) {
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

extern "C" char* start_ftdi_backend(const char* serial_number, void (*on_data)(const char* packet_json)) {
    try {
        return start_backend(serial_number, get_ftdi_backend, on_data);
    } catch (const std::exception& e) {
        return to_ffi_result({{"status", 500}, {"error", e.what()}});
    }
}

extern "C" char* stop_ftdi_backend(const char* serial_number) {
    try {
        return stop_backend(serial_number, get_ftdi_backend); 
    } catch (const std::exception& e) {
        return to_ffi_result({{"status", 500}, {"error", e.what()}});
    }
}

extern "C" char* destroy_ftdi_backend(const char* serial_number) {
    try {
        char* result = destroy_backend(serial_number, get_ftdi_backend);
        g_ftdi_backends.erase(serial_number);
        return result;
    } catch (const std::exception& e) {
        return to_ffi_result({{"status", 500}, {"error", e.what()}});
    }
}

// For tests only

void reset_ble_backends() {
    g_ble_backends.clear();
    g_ble_factory = [](const std::string& device_uuid) {
        return std::make_shared<ndx::BleBackend>(device_uuid, ndx::create_ble_provider());
    };
}

void reset_ftdi_backends() {
    g_ftdi_backends.clear();
    g_ftdi_factory = [](const std::string& serial_number) {
        return std::make_shared<ndx::FtdiBackend>(serial_number);
    };
}

void set_ble_factory(BleFactory factory) {
    g_ble_factory = factory;
}

void set_ftdi_factory(FtdiFactory factory) {
    g_ftdi_factory = factory;
}