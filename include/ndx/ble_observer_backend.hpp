#pragma once
#include "acquisition_backend.hpp"
#include "ble_provider.hpp"
#include <string>

namespace ndx {

class BleObserverBackend : public AcquisitionBackend {
public:
  explicit BleObserverBackend(const std::string& device_id, std::unique_ptr<BleProvider> provider);
  virtual void start(ndx::OnAdvertisementCallback on_advertisement);
  void stop() override;
  std::string name() const override { return "BleObserverBackend"; }

private:
  std::unique_ptr<BleProvider> provider_;
};

}
