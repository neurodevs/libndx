#pragma once
#include <algorithm>
#include <cstring>
#include <string>
#include <nlohmann/json.hpp>

#include "ndx/acquisition_backend.hpp"
#include <cstdint>

static bool is_valid_uuid(const std::string& uuid) {
    const int num_hyphens = std::count(uuid.begin(), uuid.end(), '-');
    return uuid.size() == 36 && num_hyphens == 4;
}

static bool is_valid_serial(const std::string& serial_number) {
    return serial_number.size() == 8;
}

static char* to_ffi_result(const nlohmann::json& j) {
    return strdup(j.dump().c_str());
}

template<typename GetFn>
static char* start_backend(const char* device_id, GetFn get_backend, const CharCallback* callbacks, size_t num_callbacks) {
    auto backend = get_backend(device_id);
    if (backend) {
        ndx::CharCallbacks char_callbacks;
        for (size_t i = 0; i < num_callbacks; i++) {
            on_data_fn fn = callbacks[i].callback;
            ndx::CharCallbackEntry entry;
            entry.char_uuid = callbacks[i].char_uuid;
            if (callbacks[i].char_name) entry.char_name = callbacks[i].char_name;
            entry.on_data = [fn](const ndx::Packet& p) {
                fn(p.data.data(), p.data.size(), static_cast<double>(p.timestamp_ms));
            };
            char_callbacks.push_back(std::move(entry));
        }
        backend->start(std::move(char_callbacks), nullptr);
    }
    return to_ffi_result({{"status", 200}});
}

template<typename GetFn>
static char* stop_backend(const char* device_id, GetFn get_backend) {
    auto backend = get_backend(device_id);
    if (backend) backend->stop();
    return to_ffi_result({{"status", 200}});
}
