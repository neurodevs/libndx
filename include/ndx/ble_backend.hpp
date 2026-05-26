#pragma once
#include "acquisition_backend.hpp"
#include "ble_provider.hpp"
#include <string>

namespace ndx {

class BleBackend : public AcquisitionBackend {
public:
  explicit BleBackend(const std::string& device_id, std::unique_ptr<BleProvider> provider);
  void start(CharCallbacks callbacks) override;
  void stop() override;
  void scan_all(int duration_ms, ScanResultCallback on_complete);
  virtual int read_rssi();
  virtual void write_characteristic(const std::string& char_uuid, const uint8_t* data, size_t len);
  std::string name() const override { return "BleBackend"; }

private:
  std::unique_ptr<BleProvider> provider_;
};

}