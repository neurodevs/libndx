#pragma once
#include "acquisition_backend.hpp"
#include "ble_provider.hpp"
#include "ble_types.hpp"
#include <string>

namespace ndx {

class BleGattBackend : public AcquisitionBackend {
public:
  explicit BleGattBackend(const std::string& device_id, std::unique_ptr<BleProvider> provider);
  virtual void start(CharCallbacks callbacks, ndx::OnConnectedCallback on_connected = nullptr,
                     ndx::OnConnectionStatusCallback on_connection_status = nullptr);
  virtual void add_char_callbacks(CharCallbacks callbacks);
  void stop() override;
  virtual int read_rssi();
  virtual void set_rssi_interval(int interval_ms, std::function<void(int)> on_rssi);
  virtual void stop_rssi_interval();
  virtual void write_characteristic(const std::string& char_uuid, const uint8_t* data, size_t len);
  std::string name() const override { return "BleGattBackend"; }

private:
  void handle_connected(const Device* device);
  void handle_disconnected(bool intentional);

  std::unique_ptr<BleProvider> provider_;
  ndx::OnConnectedCallback on_connected_;
  ndx::OnConnectionStatusCallback on_connection_status_;
  bool is_reconnecting_ = false;
  int rssi_interval_ms_ = 0;
  std::function<void(int)> on_rssi_;
};

}