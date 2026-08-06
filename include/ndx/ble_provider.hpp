#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "ndx/ble_types.hpp"

namespace ndx {

class BleProvider {
public:
  virtual ~BleProvider() = default;
  virtual bool is_powered_on() = 0;
  virtual void discover_ble_uuid(const std::string& name_prefix, std::function<void(const std::string& uuid)> on_discovered) = 0;
  virtual void scan_for_peripheral(const std::string& uuid, CharCallbacks callbacks, ndx::OnConnectedCallback on_connected,
                                   ndx::OnDisconnectedCallback on_disconnected = nullptr) = 0;
  virtual void start_advertisement_listen(const std::string& uuid, ndx::OnDataCallback on_data) = 0;
  virtual void stop_advertisement_listen() = 0;
  virtual void add_char_callbacks(CharCallbacks callbacks) = 0;
  virtual int read_rssi() = 0;
  virtual void set_rssi_interval(int interval_ms, std::function<void(int)> on_rssi) = 0;
  virtual void stop_rssi_interval() = 0;
  virtual void write_characteristic(const std::string& char_uuid, const uint8_t* data, size_t len) = 0;
  virtual void disconnect_peripheral(const std::string& uuid) = 0;
};

std::unique_ptr<BleProvider> create_ble_provider();

}
