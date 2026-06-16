#include "ndx/bt_backend.hpp"
#include <stdexcept>

namespace ndx {

BtBackend::BtBackend(const std::string& device_address, int port,
                     std::unique_ptr<BtProvider> provider)
    : AcquisitionBackend(device_address), port_(port), provider_(std::move(provider)) {}

void BtBackend::start(OnDataCallback on_data, OnConnectedCallback on_connected) {
  AcquisitionBackend::start({});

  if (!provider_->is_powered_on()) {
    throw std::runtime_error("BtBackend: Bluetooth is not powered on");
  }

  provider_->connect(device_id_, port_, std::move(on_data), std::move(on_connected));
}

void BtBackend::start(CharCallbacks callbacks, OnConnectedCallback on_connected) {
  throw std::runtime_error("BtBackend: use start(OnDataCallback) for serial Bluetooth");
}

void BtBackend::stop() {
  AcquisitionBackend::stop();
  provider_->disconnect();
}

}
