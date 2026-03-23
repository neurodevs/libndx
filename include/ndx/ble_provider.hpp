#pragma once
#include <memory>

namespace ndx {

class BleProvider {
public:
  virtual ~BleProvider() = default;
  virtual bool isPoweredOn() = 0;
};

std::unique_ptr<BleProvider> createBleProvider();

}
