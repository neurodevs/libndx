#pragma once
#include <algorithm>
#include <cstring>
#include <string>
#include <nlohmann/json.hpp>
#include "ndx/acquisition_backend.hpp"

static bool is_valid_mac(const std::string& address) {
    const int num_colons = std::count(address.begin(), address.end(), ':');
    return address.size() == 17 && num_colons == 5;
}

static bool is_valid_serial(const std::string& serial_number) {
    return serial_number.size() == 8;
}

static char* to_ffi_result(const nlohmann::json& j) {
    return strdup(j.dump().c_str());
}

template<typename GetFn>
static char* startBackend(const char* id_str, GetFn getBackend) {
    int id = std::stoi(id_str);
    auto backend = getBackend(id);
    if (backend) backend->start([](const ndx::Packet&) {});
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
