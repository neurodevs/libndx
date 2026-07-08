#pragma once
#include "acquisition_backend.hpp"
#include "ble_provider.hpp"
#include "ble_types.hpp"
#include <string>

namespace ndx {

class BleBackend : public AcquisitionBackend {
public:
  explicit BleBackend(const std::string& device_id, std::unique_ptr<BleProvider> provider);
  virtual void start(CharCallbacks callbacks, ndx::OnConnectedCallback on_connected = nullptr);
  virtual void add_char_callbacks(CharCallbacks callbacks);
  void stop() override;
  virtual int read_rssi();
  virtual void set_rssi_interval(int interval_ms, std::function<void(int)> on_rssi);
  virtual void stop_rssi_interval();
  virtual void write_characteristic(const std::string& char_uuid, const uint8_t* data, size_t len);
  std::string name() const override { return "BleBackend"; }

private:
  std::unique_ptr<BleProvider> provider_;
};

}