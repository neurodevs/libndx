#pragma once

#include <memory>
#include <functional>
#include <string>

#include "ndx/ble_backend.hpp"
#include "ndx/ftdi_backend.hpp"

extern "C" {

char* create_ble_backend(const char* device_uuid);
char* start_ble_backend(const char* device_uuid, void (*on_data)(const char* packet_json));
char* write_ble_characteristic(const char* device_uuid, const char* char_uuid, const char* value);
char* read_ble_rssi(const char* device_uuid);
char* stop_ble_backend(const char* device_uuid);
char* destroy_ble_backend(const char* device_uuid);

char* create_ftdi_backend(const char* serial_number);
char* start_ftdi_backend(const char* serial_number, void (*on_data)(const char* packet_json));
char* stop_ftdi_backend(const char* serial_number);
char* destroy_ftdi_backend(const char* serial_number);

}

using BleFactory = std::function<std::shared_ptr<ndx::BleBackend>(const std::string&)>;
using FtdiFactory = std::function<std::shared_ptr<ndx::FtdiBackend>(const std::string&)>;

#ifdef NDX_TESTING
std::shared_ptr<ndx::BleBackend> get_ble_backend(const std::string& device_uuid);
std::shared_ptr<ndx::FtdiBackend> get_ftdi_backend(const std::string& serial_number);
void reset_ble_backends();
void reset_ftdi_backends();
void set_ble_factory(BleFactory factory);
void set_ftdi_factory(FtdiFactory factory);
#endif
