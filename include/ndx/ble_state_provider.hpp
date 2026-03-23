#pragma once
#include <memory>

namespace ndx {

class BleStateProvider {
public:
  virtual ~BleStateProvider() = default;
  virtual bool isPoweredOn() = 0;
};

std::unique_ptr<BleStateProvider> createBleStateProvider();

}
