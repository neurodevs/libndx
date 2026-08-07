#include "ndx/acquisition_backend.hpp"
#include "ndx/ble_observer_backend.hpp"
#include <stdexcept>

namespace ndx {

BleObserverBackend::BleObserverBackend(const std::string& device_id, std::unique_ptr<BleProvider> provider)
    : AcquisitionBackend(device_id), provider_(std::move(provider)) {}

void BleObserverBackend::start(ndx::OnDataCallback on_advertisement) {
  AcquisitionBackend::start();

  if (!provider_->is_powered_on()) {
    throw std::runtime_error("BleObserverBackend: Bluetooth is not powered on");
  }

  provider_->start_advertisement_listen(device_id_, std::move(on_advertisement));
}

void BleObserverBackend::stop() {
  AcquisitionBackend::stop();

  provider_->stop_advertisement_listen();
}

}
