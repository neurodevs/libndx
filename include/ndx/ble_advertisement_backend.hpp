#pragma once
#include "acquisition_backend.hpp"
#include "ble_provider.hpp"
#include <string>

namespace ndx {

class BleAdvertisementBackend : public AcquisitionBackend {
public:
  explicit BleAdvertisementBackend(const std::string& device_id, std::unique_ptr<BleProvider> provider);
  virtual void start(ndx::OnDataCallback on_data);
  void stop() override;
  std::string name() const override { return "BleAdvertisementBackend"; }

private:
  std::unique_ptr<BleProvider> provider_;
};

}
