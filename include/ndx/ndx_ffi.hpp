#pragma once

#include <memory>
#include <functional>
#include <string>
#include <cstdint>
#include <cstddef>

#include "ndx/acquisition_backend.hpp"
#include "ndx/ble_backend.hpp"
#include "ndx/ftdi_backend.hpp"

extern "C" {

struct Peripheral {
  const char* uuid;
  const char* name;
};

typedef void (*on_connected_fn)(const Peripheral*);
typedef void (*on_data_fn)(const uint8_t* data, size_t len, double timestamp_ms);
typedef void (*on_rssi_fn)(int rssi);

struct CharCallback {
  const char* char_uuid;  // required
  const char* char_name;  // optional, may be NULL
  on_data_fn  callback;
};

char* create_ble_backend(const char* device_uuid);
char* start_ble_backend(const char* device_uuid, on_connected_fn on_connected, const CharCallback* callbacks, size_t num_callbacks);
char* write_ble_characteristic(const char* device_uuid, const char* char_uuid, const char* value);
char* set_ble_rssi_interval(const char* device_uuid, int interval_ms, on_rssi_fn on_rssi);
char* stop_ble_rssi_interval(const char* device_uuid);
char* stop_ble_backend(const char* device_uuid);

char* create_ftdi_backend(const char* serial_number);
char* start_ftdi_backend(const char* serial_number, void (*on_data)(const uint8_t* data, size_t len, double timestamp_ms));
char* stop_ftdi_backend(const char* serial_number);

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
