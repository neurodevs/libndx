#include "ndx/acquisition_backend.hpp"
#include "ndx/ble_gatt_backend.hpp"
#include <stdexcept>

namespace ndx {

BleGattBackend::BleGattBackend(const std::string& device_id, std::unique_ptr<BleProvider> provider)
    : AcquisitionBackend(device_id), provider_(std::move(provider)) {}

void BleGattBackend::start(CharCallbacks callbacks, ndx::OnConnectedCallback on_connected,
                       ndx::OnDisconnectedCallback on_disconnected) {
  AcquisitionBackend::start();

  if (!provider_->is_powered_on()) {
    throw std::runtime_error("BleGattBackend: Bluetooth is not powered on");
  }

  provider_->scan_for_peripheral(device_id_, std::move(callbacks), std::move(on_connected),
                                 std::move(on_disconnected));
}

void BleGattBackend::add_char_callbacks(CharCallbacks callbacks) {
  provider_->add_char_callbacks(std::move(callbacks));
}

void BleGattBackend::stop() {
  AcquisitionBackend::stop();
  provider_->disconnect_peripheral(device_id_);
}

int BleGattBackend::read_rssi() {
  return provider_->read_rssi();
}

void BleGattBackend::set_rssi_interval(int interval_ms, std::function<void(int)> on_rssi) {
  provider_->set_rssi_interval(interval_ms, std::move(on_rssi));
}

void BleGattBackend::stop_rssi_interval() {
  provider_->stop_rssi_interval();
}

void BleGattBackend::write_characteristic(const std::string& char_uuid, const uint8_t* data, size_t len) {
  provider_->write_characteristic(char_uuid, data, len);
}

}