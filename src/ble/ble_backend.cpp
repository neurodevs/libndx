#include "ndx/acquisition_backend.hpp"
#include "ndx/ble_backend.hpp"
#include <stdexcept>

namespace ndx {

BleBackend::BleBackend(const std::string& device_id, std::unique_ptr<BleProvider> provider)
    : AcquisitionBackend(device_id), provider_(std::move(provider)) {}

void BleBackend::start(CharCallbacks callbacks, ndx::OnConnectedCallback on_connected) {
  AcquisitionBackend::start();

  if (!provider_->is_powered_on()) {
    throw std::runtime_error("BleBackend: Bluetooth is not powered on");
  }

  provider_->scan_for_peripheral(device_id_, std::move(callbacks), std::move(on_connected));
}

void BleBackend::stop() {
  AcquisitionBackend::stop();
  provider_->disconnect_peripheral(device_id_);
}

int BleBackend::read_rssi() {
  return provider_->read_rssi();
}

void BleBackend::set_rssi_interval(int interval_ms, std::function<void(int)> on_rssi) {
  provider_->set_rssi_interval(interval_ms, std::move(on_rssi));
}

void BleBackend::stop_rssi_interval() {
  provider_->stop_rssi_interval();
}

void BleBackend::write_characteristic(const std::string& char_uuid, const uint8_t* data, size_t len) {
  provider_->write_characteristic(char_uuid, data, len);
}

}