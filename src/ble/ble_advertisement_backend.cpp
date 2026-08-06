#include "ndx/acquisition_backend.hpp"
#include "ndx/ble_advertisement_backend.hpp"
#include <stdexcept>

namespace ndx {

BleAdvertisementBackend::BleAdvertisementBackend(const std::string& device_id, std::unique_ptr<BleProvider> provider)
    : AcquisitionBackend(device_id), provider_(std::move(provider)) {}

void BleAdvertisementBackend::start(ndx::OnDataCallback on_data) {
  AcquisitionBackend::start();

  if (!provider_->is_powered_on()) {
    throw std::runtime_error("BleAdvertisementBackend: Bluetooth is not powered on");
  }

  provider_->start_advertisement_listen(device_id_, std::move(on_data));
}

void BleAdvertisementBackend::stop() {
  AcquisitionBackend::stop();

  provider_->stop_advertisement_listen();
}

}
