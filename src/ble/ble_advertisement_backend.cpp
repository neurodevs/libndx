#include "ndx/acquisition_backend.hpp"
#include "ndx/ble_advertisement_backend.hpp"

namespace ndx {

BleAdvertisementBackend::BleAdvertisementBackend(const std::string& device_id, std::unique_ptr<BleProvider> provider)
    : AcquisitionBackend(device_id), provider_(std::move(provider)) {}

void BleAdvertisementBackend::start(ndx::OnDataCallback on_data) {
  AcquisitionBackend::start();
  provider_->listen_for_advertisements(device_id_, std::move(on_data));
}

void BleAdvertisementBackend::stop() {
  AcquisitionBackend::stop();
}

}
