#include "ndx/ble_provider.hpp"

namespace ndx {

class BleProviderImpl : public BleProvider {
public:
  bool isPoweredOn() override { return false; }
  void scanForPeripheral(const std::string&, ndx::DidConnectCallback) override {}
};

std::unique_ptr<BleProvider> createBleProvider() {
  return std::make_unique<BleProviderImpl>();
}

}
