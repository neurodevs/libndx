#pragma once
#include "acquisition_backend.hpp"
#include <string>

namespace ndx {

class BleBackend : public AcquisitionBackend {
public:
  explicit BleBackend(const std::string& device_id);
  void start(PacketCallback cb) override;
  std::string name() const override { return "BleBackend"; }

protected:
  virtual bool isBluetoothPoweredOn();
};

}