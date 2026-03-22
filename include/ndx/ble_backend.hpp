#pragma once
#include "acquisition_backend.hpp"
#include "ble_state_provider.hpp"
#include <string>

namespace ndx {

class BleBackend : public AcquisitionBackend {
public:
  explicit BleBackend(const std::string& device_id, std::unique_ptr<BleStateProvider> provider);
  void start(PacketCallback cb) override;
  std::string name() const override { return "BleBackend"; }

private:
  std::unique_ptr<BleStateProvider> provider_;
};

}