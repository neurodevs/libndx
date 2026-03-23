#include "ndx/ble_state_provider.hpp"

namespace ndx {

bool BleStateProviderImpl::isPoweredOn() {
  return false;
}

std::unique_ptr<BleStateProvider> createBleStateProvider() {
  return std::make_unique<BleStateProviderImpl>();
}

}