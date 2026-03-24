#include "ndx/ble_backend.hpp"
#include <stdexcept>

namespace ndx {

BleBackend::BleBackend(const std::string& device_id, std::unique_ptr<BleProvider> provider)
  : AcquisitionBackend(device_id), provider_(std::move(provider)) {}

  void BleBackend::start(PacketCallback cb) {
  AcquisitionBackend::start(cb);

  if (!provider_->isPoweredOn()) {
    throw std::runtime_error("BleBackend: Bluetooth is not powered on");
  }

provider_->scanForPeripheral(device_id_);
}

}