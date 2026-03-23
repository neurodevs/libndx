#pragma once
#include <memory>

namespace ndx {

class BleStateProvider {
public:
  virtual ~BleStateProvider() = default;
  virtual bool isPoweredOn() = 0;
};

class BleStateProviderImpl : public BleStateProvider {
public:
  bool isPoweredOn() override;
};

std::unique_ptr<BleStateProvider> createBleStateProvider();

}
