#include <cstring>
#include <cstdint>
#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include <nlohmann/json.hpp>

#include "ndx/ndx_ffi.hpp"
#include "ndx/ndx_ffi_impl.hpp"
#include "ndx/acquisition_backend.hpp"
#include "ndx/ble_gatt_backend.hpp"
#include "ndx/ble_provider.hpp"
#include "ndx/usb_backend.hpp"
#include "ndx/usb_provider.hpp"

static std::string to_hex(const std::vector<uint8_t>& bytes) {
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (uint8_t b : bytes) out << std::setw(2) << static_cast<int>(b);
    return out.str();
}

static std::unordered_map<std::string, std::shared_ptr<ndx::BleGattBackend>> g_ble_gatt_backends;
static std::unordered_map<std::string, std::shared_ptr<ndx::BleObserverBackend>> g_ble_observer_backends;
static std::unordered_map<std::string, std::unique_ptr<ndx::BleProvider>> g_ble_scanners;
static std::unordered_map<std::string, std::shared_ptr<ndx::UsbBackend>> g_usb_backends;

static BleGattFactory g_ble_gatt_factory = [](const std::string& device_uuid) {
    return std::make_shared<ndx::BleGattBackend>(device_uuid, ndx::create_ble_provider());
};

static BleObserverFactory g_ble_observer_factory = [](const std::string& device_uuid) {
    return std::make_shared<ndx::BleObserverBackend>(device_uuid, ndx::create_ble_provider());
};

static BleProviderFactory g_ble_provider_factory = []() {
    return ndx::create_ble_provider();
};

std::shared_ptr<ndx::BleGattBackend> get_ble_gatt_backend(const std::string& device_uuid) {
    auto it = g_ble_gatt_backends.find(device_uuid);
    return it != g_ble_gatt_backends.end() ? it->second : nullptr;
}

std::shared_ptr<ndx::BleObserverBackend> get_ble_observer_backend(const std::string& device_uuid) {
    auto it = g_ble_observer_backends.find(device_uuid);
    return it != g_ble_observer_backends.end() ? it->second : nullptr;
}

static bool is_ble_gatt_registered(const std::string& device_uuid) {
    return g_ble_gatt_backends.count(device_uuid) > 0;
}

static bool is_ble_observer_registered(const std::string& device_uuid) {
    return g_ble_observer_backends.count(device_uuid) > 0;
}

static UsbFactory g_usb_factory = [](const std::string& serial_number) {
    return std::make_shared<ndx::UsbBackend>(serial_number, ndx::create_usb_provider());
};

std::shared_ptr<ndx::UsbBackend> get_usb_backend(const std::string& serial_number) {
    auto it = g_usb_backends.find(serial_number);
    return it != g_usb_backends.end() ? it->second : nullptr;
}

static bool is_usb_registered(const std::string& serial_number) {
    return g_usb_backends.count(serial_number) > 0;
}

extern "C" char* discover_ble_uuid(const char* name_prefix, on_discovered_fn on_discovered) {
    return try_to_run([&] {
        std::string prefix = name_prefix;
        auto provider = g_ble_provider_factory();
        auto* prov = provider.get();
        g_ble_scanners[prefix] = std::move(provider);
        prov->discover_ble_uuid(prefix, [prefix, on_discovered](const std::string& uuid) {
            auto it = g_ble_scanners.find(prefix);
            if (it != g_ble_scanners.end()) {
                g_ble_scanners[uuid] = std::move(it->second);
                g_ble_scanners.erase(prefix);
            }
            if (on_discovered) on_discovered(uuid.c_str());
        });
        return to_ffi_result({{"status", 200}});
    });
}

extern "C" char* create_ble_gatt_backend(const char* config_json) {
    return try_to_run([&] {
        auto j = nlohmann::json::parse(config_json, nullptr, false);

        if (j.is_discarded()) {
            return to_ffi_result({{"status", 400}, {"error", "malformed JSON"}});
        }

        if (!j.contains("uuid") || !j["uuid"].is_string()) {
            return to_ffi_result({{"status", 400}, {"error", "missing uuid"}});
        }

        std::string uuid = j["uuid"].get<std::string>();

        if (!is_valid_uuid(uuid)) {
            return to_ffi_result({{"status", 400}, {"error", "invalid uuid"}});
        }

        if (is_ble_gatt_registered(uuid)) {
            return to_ffi_result({{"status", 400}, {"error", "uuid already registered"}});
        }

        auto sit = g_ble_scanners.find(uuid);
        
        if (sit != g_ble_scanners.end()) {
            g_ble_gatt_backends[uuid] = std::make_shared<ndx::BleGattBackend>(uuid, std::move(sit->second));
            g_ble_scanners.erase(sit);
        } else {
            g_ble_gatt_backends[uuid] = g_ble_gatt_factory(uuid);
        }
        return to_ffi_result({{"status", 200}});
    });
}

extern "C" char* start_ble_gatt_backend(const char* device_uuid, on_connected_fn on_connected, const CharCallback* callbacks, size_t num_callbacks) {
    return try_to_run([&] {
        auto backend = get_ble_gatt_backend(device_uuid);
        if (!backend) return to_ffi_result(BACKEND_NOT_FOUND);
        ndx::CharCallbacks cbs;
        for (size_t i = 0; i < num_callbacks; ++i) {
            auto& c = callbacks[i];
            cbs.push_back({c.char_uuid ? c.char_uuid : "", c.char_name ? c.char_name : "", [fn = c.callback](const ndx::Packet& p) {
                fn(p.data.data(), p.data.size(), p.timestamp_sec);
            }});
        }
        ndx::OnConnectedCallback cb = on_connected
            ? ndx::OnConnectedCallback([on_connected](const ndx::Device* p) {
                on_connected(p ? p->id.c_str() : nullptr, p ? p->name.c_str() : nullptr);
              })
            : nullptr;
        backend->start(std::move(cbs), std::move(cb));
        return to_ffi_result({{"status", 200}});
    });
}

extern "C" char* register_ble_gatt_char_callbacks(const char* device_uuid, const CharCallback* callbacks, size_t num_callbacks) {
    return try_to_run([&] {
        auto backend = get_ble_gatt_backend(device_uuid);
        if (!backend) return to_ffi_result(BACKEND_NOT_FOUND);
        ndx::CharCallbacks cbs;
        for (size_t i = 0; i < num_callbacks; ++i) {
            auto& c = callbacks[i];
            cbs.push_back({c.char_uuid ? c.char_uuid : "", c.char_name ? c.char_name : "", [fn = c.callback](const ndx::Packet& p) {
                fn(p.data.data(), p.data.size(), p.timestamp_sec);
            }});
        }
        backend->add_char_callbacks(std::move(cbs));
        return to_ffi_result({{"status", 200}});
    });
}

extern "C" char* write_ble_gatt_char(const char* device_uuid, const char* char_uuid, const char* cmd) {
    return try_to_run([&] {
        auto backend = get_ble_gatt_backend(device_uuid);
        if (!backend) return to_ffi_result(BACKEND_NOT_FOUND);
        size_t cmd_len = strlen(cmd);
        std::vector<char> buf(cmd_len + 2);
        buf[0] = (char)(cmd_len + 1);
        memcpy(buf.data() + 1, cmd, cmd_len);
        buf[1 + cmd_len] = '\n';
        backend->write_characteristic(char_uuid, reinterpret_cast<const uint8_t*>(buf.data()), buf.size());
        return to_ffi_result({{"status", 200}});
    });
}

extern "C" char* start_ble_gatt_rssi_polling(const char* device_uuid, int interval_ms, on_rssi_fn on_rssi) {
    return try_to_run([&] {
        auto backend = get_ble_gatt_backend(device_uuid);
        if (!backend) return to_ffi_result(BACKEND_NOT_FOUND);
        backend->set_rssi_interval(interval_ms, [on_rssi](int rssi) { on_rssi(rssi); });
        return to_ffi_result({{"status", 200}});
    });
}

extern "C" char* stop_ble_gatt_rssi_polling(const char* device_uuid) {
    return try_to_run([&] {
        auto backend = get_ble_gatt_backend(device_uuid);
        if (!backend) return to_ffi_result(BACKEND_NOT_FOUND);
        backend->stop_rssi_interval();
        return to_ffi_result({{"status", 200}});
    });
}

extern "C" char* stop_ble_gatt_backend(const char* device_uuid)  {
    return try_to_run([&] {
        auto result = stop_backend(device_uuid, get_ble_gatt_backend);
        g_ble_gatt_backends.erase(device_uuid);
        return result;
    });
}

extern "C" char* create_ble_observer_backend(const char* config_json) {
    return try_to_run([&] {
        auto j = nlohmann::json::parse(config_json, nullptr, false);

        if (j.is_discarded()) {
            return to_ffi_result({{"status", 400}, {"error", "malformed JSON"}});
        }

        if (!j.contains("uuid") || !j["uuid"].is_string()) {
            return to_ffi_result({{"status", 400}, {"error", "missing uuid"}});
        }

        std::string uuid = j["uuid"].get<std::string>();

        if (!is_valid_uuid(uuid)) {
            return to_ffi_result({{"status", 400}, {"error", "invalid uuid"}});
        }

        if (is_ble_observer_registered(uuid)) {
            return to_ffi_result({{"status", 400}, {"error", "uuid already registered"}});
        }

        g_ble_observer_backends[uuid] = g_ble_observer_factory(uuid);
        return to_ffi_result({{"status", 200}});
    });
}

extern "C" char* start_ble_observer_backend(const char* device_uuid, on_advertisement_fn on_advertisement) {
    return try_to_run([&] {
        auto backend = get_ble_observer_backend(device_uuid);
        if (!backend) return to_ffi_result(BACKEND_NOT_FOUND);

        backend->start([on_advertisement](const ndx::Advertisement& a) {
            nlohmann::json service_data = nlohmann::json::object();
            for (const auto& sd : a.service_data) service_data[sd.uuid] = to_hex(sd.data);

            nlohmann::json j = {
                {"localName", a.local_name},
                {"companyId", a.company_id ? nlohmann::json(*a.company_id) : nlohmann::json(nullptr)},
                {"manufacturerData", to_hex(a.manufacturer_data)},
                {"serviceUuids", a.service_uuids},
                {"serviceData", service_data},
                {"rssi", a.rssi ? nlohmann::json(*a.rssi) : nlohmann::json(nullptr)},
                {"txPowerLevel", a.tx_power_level ? nlohmann::json(*a.tx_power_level) : nlohmann::json(nullptr)},
                {"isConnectable", a.is_connectable},
                {"timestampSec", a.timestamp_sec},
            };
            on_advertisement(j.dump().c_str());
        });

        return to_ffi_result({{"status", 200}});
    });
}

extern "C" char* stop_ble_observer_backend(const char* device_uuid) {
    return try_to_run([&] {
        auto backend = get_ble_observer_backend(device_uuid);
        if (!backend) return to_ffi_result(BACKEND_NOT_FOUND);
        
        backend->stop();
        g_ble_observer_backends.erase(device_uuid);

        return to_ffi_result({{"status", 200}});
    });
}

extern "C" char* create_usb_backend(const char* config_json) {
    return try_to_run([&] {
        auto j = nlohmann::json::parse(config_json, nullptr, false);

        if (j.is_discarded()) {
            return to_ffi_result({{"status", 400}, {"error", "malformed JSON"}});
        }

        std::string serial_number = j["serial_number"].get<std::string>();

        if (!is_valid_serial(serial_number)) {
            return to_ffi_result({{"status", 400}, {"error", "invalid serial number"}});
        }

        if (is_usb_registered(serial_number)) {
            return to_ffi_result({{"status", 400}, {"error", "serial number already registered"}});
        }

        g_usb_backends[serial_number] = g_usb_factory(serial_number);
        return to_ffi_result({{"status", 200}});
    });
}

extern "C" char* start_usb_backend(const char* serial_number, void (*on_data)(const uint8_t* data, size_t len, double timestamp_sec)) {
    return try_to_run([&] {
        auto backend = get_usb_backend(serial_number);
        if (backend) {
            backend->start([fn = on_data](const ndx::Packet& p) {
                fn(p.data.data(), p.data.size(), p.timestamp_sec);
            });
        }
        return to_ffi_result({{"status", 200}});
    });
}

extern "C" char* write_usb_backend(const char* serial_number, const char* value) {
    return try_to_run([&] {
        auto backend = get_usb_backend(serial_number);
        if (!backend) return to_ffi_result(BACKEND_NOT_FOUND);
        backend->write(reinterpret_cast<const uint8_t*>(value), strlen(value));
        return to_ffi_result({{"status", 200}});
    });
}

extern "C" char* stop_usb_backend(const char* serial_number) {
    return try_to_run([&] {
        return stop_backend(serial_number, get_usb_backend);
    });
}

// For tests only

void reset_ble_gatt_backends() {
    g_ble_gatt_backends.clear();
    g_ble_scanners.clear();
    g_ble_gatt_factory = [](const std::string& device_uuid) {
        return std::make_shared<ndx::BleGattBackend>(device_uuid, ndx::create_ble_provider());
    };
    g_ble_provider_factory = []() { return ndx::create_ble_provider(); };
}

void reset_ble_observer_backends() {
    g_ble_observer_backends.clear();
    g_ble_observer_factory = [](const std::string& device_uuid) {
        return std::make_shared<ndx::BleObserverBackend>(device_uuid, ndx::create_ble_provider());
    };
}


void reset_usb_backends() {
    g_usb_backends.clear();
    g_usb_factory = [](const std::string& serial_number) {
        return std::make_shared<ndx::UsbBackend>(serial_number, ndx::create_usb_provider());
    };
}

void set_ble_gatt_factory(BleGattFactory factory) {
    g_ble_gatt_factory = factory;
}

void set_ble_observer_factory(BleObserverFactory factory) {
    g_ble_observer_factory = std::move(factory);
}

void set_ble_provider_factory(BleProviderFactory factory) {
    g_ble_provider_factory = factory;
}

void set_usb_factory(UsbFactory factory) {
    g_usb_factory = factory;
}