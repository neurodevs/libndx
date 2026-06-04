#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ndx/acquisition_backend.hpp"

namespace ndx {

struct PeripheralInfo {
  std::string uuid;
  std::string name;
};

using ScanResultCallback = std::function<void(const std::vector<PeripheralInfo>&)>;

class BleProvider {
public:
  virtual ~BleProvider() = default;
  virtual bool is_powered_on() = 0;
  virtual void scan_for_peripheral(const std::string& uuid, CharCallbacks callbacks, ndx::OnConnectedCallback on_connected) = 0;
  virtual void scan_all(int duration_ms, ScanResultCallback on_complete) = 0;
  virtual int read_rssi() = 0;
  virtual void set_rssi_interval(int interval_ms, std::function<void(int)> on_rssi) = 0;
  virtual void stop_rssi_interval() = 0;
  virtual void write_characteristic(const std::string& char_uuid, const uint8_t* data, size_t len) = 0;
  virtual void disconnect_peripheral(const std::string& uuid) = 0;
};

std::unique_ptr<BleProvider> create_ble_provider();

}
