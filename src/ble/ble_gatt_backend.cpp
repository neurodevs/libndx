#include "ndx/acquisition_backend.hpp"
#include "ndx/ble_gatt_backend.hpp"
#include <stdexcept>

namespace ndx {

BleGattBackend::BleGattBackend(const std::string& device_id, std::unique_ptr<BleProvider> provider)
    : AcquisitionBackend(device_id), provider_(std::move(provider)) {}


BleGattBackend::~BleGattBackend() {
  provider_.reset();
}

void BleGattBackend::start(CharCallbacks callbacks, ndx::OnConnectedCallback on_connected,
                       ndx::OnLostCallback on_lost, ndx::OnReconnectCallback on_reconnect,
                       ndx::OnDisconnectedCallback on_disconnected) {
  AcquisitionBackend::start();

  if (!provider_->is_powered_on()) {
    throw std::runtime_error("BleGattBackend: Bluetooth is not powered on");
  }

  on_connected_ = std::move(on_connected);
  on_lost_ = std::move(on_lost);
  on_reconnect_ = std::move(on_reconnect);
  on_disconnected_ = std::move(on_disconnected);

  provider_->scan_for_peripheral(
      device_id_, std::move(callbacks), [this](const Device* device) { handle_connected(device); },
      [this](bool intentional) { handle_disconnected(intentional); });
}

void BleGattBackend::handle_connected(const Device* device) {
  if (!is_reconnecting_) {
    if (on_connected_) on_connected_(device);
    return;
  }

  is_reconnecting_ = false;
  if (rssi_interval_ms_) provider_->set_rssi_interval(rssi_interval_ms_, on_rssi_);
  if (on_reconnect_) on_reconnect_();
}

void BleGattBackend::handle_disconnected(bool intentional) {
  if (intentional) {
    if (on_disconnected_) on_disconnected_();
    return;
  }

  if (!is_running()) return;

  is_reconnecting_ = true;
  if (on_lost_) on_lost_();
  provider_->reconnect_peripheral(device_id_);
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
  rssi_interval_ms_ = interval_ms;
  on_rssi_ = on_rssi;
  provider_->set_rssi_interval(interval_ms, std::move(on_rssi));
}

void BleGattBackend::stop_rssi_interval() {
  rssi_interval_ms_ = 0;
  on_rssi_ = nullptr;
  provider_->stop_rssi_interval();
}

void BleGattBackend::write_characteristic(const std::string& char_uuid, const uint8_t* data, size_t len) {
  provider_->write_characteristic(char_uuid, data, len);
}

}