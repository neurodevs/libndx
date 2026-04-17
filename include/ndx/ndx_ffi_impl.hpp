#pragma once
#include <algorithm>
#include <cstring>
#include <string>
#include <nlohmann/json.hpp>

#include "ndx/acquisition_backend.hpp"

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
static char* start_backend(const char* device_id, GetFn get_backend, void (*on_data)(const char*)) {
    auto backend = get_backend(device_id);
    if (backend) backend->start([on_data](const ndx::Packet& p) {
        auto j = nlohmann::json{{"data", p.data}, {"timestamp_ms", p.timestamp_ms}};
        on_data(j.dump().c_str());
    });
    return to_ffi_result({{"status", 200}});
}

template<typename GetFn>
static char* stop_backend(const char* device_id, GetFn get_backend) {
    auto backend = get_backend(device_id);
    if (backend) backend->stop();
    return to_ffi_result({{"status", 200}});
}

template<typename GetFn>
static char* destroy_backend(const char* device_id, GetFn get_backend) {
    auto backend = get_backend(device_id);
    if (backend) backend->destroy();
    return to_ffi_result({{"status", 200}});
}
