#include "ndx/bluetooth_backend.hpp"
#include <stdexcept>

namespace ndx {

BluetoothBackend::BluetoothBackend(const std::string& device_address, int port,
                                   std::unique_ptr<BluetoothProvider> provider)
    : AcquisitionBackend(device_address), port_(port), provider_(std::move(provider)) {}

void BluetoothBackend::start(OnDataCallback on_data, OnConnectedCallback on_connected) {
  AcquisitionBackend::start();

  if (!provider_->is_powered_on()) {
    throw std::runtime_error("BluetoothBackend: Bluetooth is not powered on");
  }

  provider_->connect(device_id_, port_, std::move(on_data), std::move(on_connected));
}

void BluetoothBackend::stop() {
  AcquisitionBackend::stop();
  provider_->disconnect();
}

}
