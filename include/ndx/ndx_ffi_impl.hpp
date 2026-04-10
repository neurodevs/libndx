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
static char* startBackend(const char* id_str, GetFn getBackend, void (*on_data)(const char*)) {
    int id = std::stoi(id_str);
    auto backend = getBackend(id);
    if (backend) backend->start([on_data](const ndx::Packet& p) {
        auto j = nlohmann::json{{"data", p.data}, {"timestamp_ms", p.timestamp_ms}};
        on_data(j.dump().c_str());
    });
    return to_ffi_result({{"status", 200}, {"id", id_str}});
}

template<typename GetFn>
static char* stopBackend(const char* id_str, GetFn getBackend) {
    int id = std::stoi(id_str);
    auto backend = getBackend(id);
    if (backend) backend->stop();
    return to_ffi_result({{"status", 200}, {"id", id_str}});
}

template<typename GetFn>
static char* destroyBackend(const char* id_str, GetFn getBackend) {
    int id = std::stoi(id_str);
    auto backend = getBackend(id);
    if (backend) backend->destroy();
    return to_ffi_result({{"status", 200}, {"id", id_str}});
}
