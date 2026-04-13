#include "ndx/ble_backend.hpp"
#include <stdexcept>

namespace ndx {

BleBackend::BleBackend(const std::string& device_id, std::unique_ptr<BleProvider> provider)
    : AcquisitionBackend(device_id), provider_(std::move(provider)) {}

void BleBackend::start(OnDataCallback cb) {
  AcquisitionBackend::start(cb);

  if (!provider_->isPoweredOn()) {
    throw std::runtime_error("BleBackend: Bluetooth is not powered on");
  }

  provider_->scanForPeripheral(device_id_, cb);
}

void BleBackend::scanAll(int duration_ms, ScanResultCallback on_complete) {
  provider_->scanAll(duration_ms, on_complete);
}

int BleBackend::getRssi() {
  return provider_->getRssi();
}

}