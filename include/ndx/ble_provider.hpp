#pragma once
#include <functional>
#include <memory>
#include <string>

namespace ndx {

using DidConnectCallback = std::function<void()>;

class BleProvider {
public:
  virtual ~BleProvider() = default;
  virtual bool isPoweredOn() = 0;
  virtual void scanForPeripheral(const std::string& id, DidConnectCallback cb) = 0;
};

std::unique_ptr<BleProvider> createBleProvider();

}
