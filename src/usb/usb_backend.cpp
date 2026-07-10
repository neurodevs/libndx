#include "ndx/usb_backend.hpp"
#include <stdexcept>

namespace ndx {

UsbBackend::UsbBackend(const std::string& device_id, std::unique_ptr<UsbProvider> provider)
  : AcquisitionBackend(device_id), provider_(std::move(provider)) {}

void UsbBackend::start(OnDataCallback on_data, OnConnectedCallback on_connected, int wait_after_connect_ms) {
  AcquisitionBackend::start();
  provider_->connect(device_id_, std::move(on_data), std::move(on_connected), wait_after_connect_ms);
}

void UsbBackend::stop() {
  AcquisitionBackend::stop();
  provider_->disconnect();
}

bool UsbBackend::write(const uint8_t* data, size_t len) {
  return provider_->write(data, len);
}

}
