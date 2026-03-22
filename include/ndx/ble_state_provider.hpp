#pragma once

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

}
