#include "ndx/ble_provider.hpp"

namespace ndx {

class BleProviderImpl : public BleProvider {
public:
  bool isPoweredOn() override { return false; }
};

std::unique_ptr<BleProvider> createBleProvider() {
  return std::make_unique<BleProviderImpl>();
}

}
