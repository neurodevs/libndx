#include <cstring>
#include <cstdint>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <memory>
#include <nlohmann/json.hpp>

#include "ndx/ndx_ffi.hpp"
#include "ndx/ndx_ffi_impl.hpp"
#include "ndx/acquisition_backend.hpp"
#include "ndx/ble_backend.hpp"
#include "ndx/ble_provider.hpp"
#include "ndx/ftdi_backend.hpp"

static std::unordered_map<std::string, std::shared_ptr<ndx::BleBackend>> g_ble_backends;
static std::unordered_map<std::string, std::unique_ptr<ndx::BleProvider>> g_ble_scanners;
static std::unordered_map<std::string, std::shared_ptr<ndx::FtdiBackend>> g_ftdi_backends;

static BleFactory g_ble_factory = [](const std::string& device_uuid) {
    return std::make_shared<ndx::BleBackend>(device_uuid, ndx::create_ble_provider());
};

static BleProviderFactory g_ble_provider_factory = []() {
    return ndx::create_ble_provider();
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

extern "C" char* discover_ble_uuid(const char* name_prefix, on_discovered_fn on_discovered) {
    try {
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
    } catch (const std::exception& e) {
        return to_ffi_result({{"status", 500}, {"error", e.what()}});
    }
}

extern "C" char* create_ble_backend(const char* config_json) {
    try {
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

        if (is_ble_registered(uuid)) {
            return to_ffi_result({{"status", 400}, {"error", "uuid already registered"}});
        }

        auto sit = g_ble_scanners.find(uuid);
        if (sit != g_ble_scanners.end()) {
            g_ble_backends[uuid] = std::make_shared<ndx::BleBackend>(uuid, std::move(sit->second));
            g_ble_scanners.erase(sit);
        } else {
            g_ble_backends[uuid] = g_ble_factory(uuid);
        }
        return to_ffi_result({{"status", 200}});
    } catch (const std::exception& e) {
        return to_ffi_result({{"status", 500}, {"error", e.what()}});
    }
}

extern "C" char* start_ble_backend(const char* device_uuid, on_connected_fn on_connected, const CharCallback* callbacks, size_t num_callbacks) {
    try {
        auto backend = get_ble_backend(device_uuid);
        if (!backend) return to_ffi_result({{"status", 400}, {"error", "backend not found"}});
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
    } catch (const std::exception& e) {
        return to_ffi_result({{"status", 500}, {"error", e.what()}});
    }
}

extern "C" char* write_ble_characteristic(const char* device_uuid, const char* char_uuid, const char* cmd) {
    try {
        auto backend = get_ble_backend(device_uuid);
        if (!backend) return to_ffi_result({{"status", 400}, {"error", "backend not found"}});
        size_t cmd_len = strlen(cmd);
        std::vector<char> buf(cmd_len + 2);
        buf[0] = (char)(cmd_len + 1);
        memcpy(buf.data() + 1, cmd, cmd_len);
        buf[1 + cmd_len] = '\n';
        backend->write_characteristic(char_uuid, reinterpret_cast<const uint8_t*>(buf.data()), buf.size());
        return to_ffi_result({{"status", 200}});
    } catch (const std::exception& e) {
        return to_ffi_result({{"status", 500}, {"error", e.what()}});
    }
}

extern "C" char* set_ble_rssi_interval(const char* device_uuid, int interval_ms, on_rssi_fn on_rssi) {
    try {
        auto backend = get_ble_backend(device_uuid);
        if (!backend) return to_ffi_result({{"status", 400}, {"error", "backend not found"}});
        backend->set_rssi_interval(interval_ms, [on_rssi](int rssi) { on_rssi(rssi); });
        return to_ffi_result({{"status", 200}});
    } catch (const std::exception& e) {
        return to_ffi_result({{"status", 500}, {"error", e.what()}});
    }
}

extern "C" char* stop_ble_rssi_interval(const char* device_uuid) {
    try {
        auto backend = get_ble_backend(device_uuid);
        if (!backend) return to_ffi_result({{"status", 400}, {"error", "backend not found"}});
        backend->stop_rssi_interval();
        return to_ffi_result({{"status", 200}});
    } catch (const std::exception& e) {
        return to_ffi_result({{"status", 500}, {"error", e.what()}});
    }
}

extern "C" char* stop_ble_backend(const char* device_uuid)  {
    try {
        auto result = stop_backend(device_uuid, get_ble_backend);
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

extern "C" char* start_ftdi_backend(const char* serial_number, void (*on_data)(const uint8_t* data, size_t len, double timestamp_sec)) {
    try {
        auto backend = get_ftdi_backend(serial_number);
        if (backend) {
            backend->start([fn = on_data](const ndx::Packet& p) {});
        }
        return to_ffi_result({{"status", 200}});
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

// For tests only

void reset_ble_backends() {
    g_ble_backends.clear();
    g_ble_scanners.clear();
    g_ble_factory = [](const std::string& device_uuid) {
        return std::make_shared<ndx::BleBackend>(device_uuid, ndx::create_ble_provider());
    };
    g_ble_provider_factory = []() { return ndx::create_ble_provider(); };
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

void set_ble_provider_factory(BleProviderFactory factory) {
    g_ble_provider_factory = factory;
}

void set_ftdi_factory(FtdiFactory factory) {
    g_ftdi_factory = factory;
}