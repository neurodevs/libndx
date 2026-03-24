#pragma once
#include "acquisition_backend.hpp"
#include <memory>
#include <string>

namespace ndx {

class BleProvider {
public:
  virtual ~BleProvider() = default;
  virtual bool isPoweredOn() = 0;
  virtual void scanForPeripheral(const std::string& id, OnDataCallback on_data) = 0;
};

std::unique_ptr<BleProvider> createBleProvider();

}
