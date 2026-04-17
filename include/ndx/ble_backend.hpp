#pragma once
#include "acquisition_backend.hpp"
#include "ble_provider.hpp"
#include <string>

namespace ndx {

class BleBackend : public AcquisitionBackend {
public:
  explicit BleBackend(const std::string& device_id, std::unique_ptr<BleProvider> provider);
  void start(OnDataCallback cb) override;
  void scanAll(int duration_ms, ScanResultCallback on_complete);
  virtual int readRssi();
  std::string name() const override { return "BleBackend"; }

private:
  std::unique_ptr<BleProvider> provider_;
};

}