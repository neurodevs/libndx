#pragma once

#include <memory>
#include <functional>
#include <string>
#include <cstdint>
#include <cstddef>

#include "ndx/acquisition_backend.hpp"
#include "ndx/ble_backend.hpp"
#include "ndx/usb_backend.hpp"

extern "C" {

typedef void (*on_connected_fn)(const char* uuid, const char* name);
typedef void (*on_data_fn)(const uint8_t* data, size_t len, double timestamp_sec);
typedef void (*on_rssi_fn)(int rssi);
typedef void (*on_discovered_fn)(const char* uuid);

struct CharCallback {
  const char* char_uuid;  // required
  const char* char_name;  // optional, may be NULL
  on_data_fn  callback;
};

char* discover_ble_uuid(const char* name_prefix, on_discovered_fn on_discovered);
char* create_ble_backend(const char* config_json);
char* start_ble_backend(const char* device_uuid, on_connected_fn on_connected, const CharCallback* callbacks, size_t num_callbacks);
char* add_ble_char_callbacks(const char* device_uuid, const CharCallback* callbacks, size_t num_callbacks);
char* write_ble_characteristic(const char* device_uuid, const char* char_uuid, const char* value);
char* set_ble_rssi_interval(const char* device_uuid, int interval_ms, on_rssi_fn on_rssi);
char* stop_ble_rssi_interval(const char* device_uuid);
char* stop_ble_backend(const char* device_uuid);

char* create_usb_backend(const char* serial_number);
char* start_usb_backend(const char* serial_number, void (*on_data)(const uint8_t* data, size_t len, double timestamp_sec));
char* stop_usb_backend(const char* serial_number);

}

using BleFactory = std::function<std::shared_ptr<ndx::BleBackend>(const std::string&)>;
using BleProviderFactory = std::function<std::unique_ptr<ndx::BleProvider>()>;
using UsbFactory = std::function<std::shared_ptr<ndx::UsbBackend>(const std::string&)>;

#ifdef NDX_TESTING
std::shared_ptr<ndx::BleBackend> get_ble_backend(const std::string& device_uuid);
std::shared_ptr<ndx::UsbBackend> get_usb_backend(const std::string& serial_number);
void reset_ble_backends();
void reset_usb_backends();
void set_ble_factory(BleFactory factory);
void set_ble_provider_factory(BleProviderFactory factory);
void set_usb_factory(UsbFactory factory);
#endif
