#include "ndx/ble_backend.hpp"
#include <stdexcept>

namespace ndx {

BleBackend::BleBackend(const std::string& device_id, std::unique_ptr<BleProvider> provider)
    : AcquisitionBackend(device_id), provider_(std::move(provider)) {}

void BleBackend::start(CharCallbacks callbacks) {
  AcquisitionBackend::start(callbacks);

  if (!provider_->is_powered_on()) {
    throw std::runtime_error("BleBackend: Bluetooth is not powered on");
  }

  provider_->scan_for_peripheral(device_id_, std::move(callbacks));
}

void BleBackend::stop() {
  AcquisitionBackend::stop();
  provider_->disconnect_peripheral(device_id_);
}

void BleBackend::scan_all(int duration_ms, ScanResultCallback on_complete) {
  provider_->scan_all(duration_ms, on_complete);
}

int BleBackend::read_rssi() {
  return provider_->read_rssi();
}

void BleBackend::write_characteristic(const std::string& char_uuid, const uint8_t* data, size_t len) {
  provider_->write_characteristic(char_uuid, data, len);
}

}