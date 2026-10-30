#include "ndx/rfcomm_backend.hpp"
#include <stdexcept>

namespace ndx {

RfcommBackend::RfcommBackend(const std::string& device_address, int port,
                                   std::unique_ptr<RfcommProvider> provider)
    : AcquisitionBackend(device_address), port_(port), provider_(std::move(provider)) {}

void RfcommBackend::start(OnDataCallback on_data, OnConnectedCallback on_connected) {
  AcquisitionBackend::start();

  if (!provider_->is_powered_on()) {
    throw std::runtime_error("RfcommBackend: Bluetooth is not powered on");
  }

  provider_->connect(device_id_, port_, std::move(on_data), std::move(on_connected));
}

void RfcommBackend::stop() {
  AcquisitionBackend::stop();
  provider_->disconnect();
}

bool RfcommBackend::write(const uint8_t* data, size_t len) {
  return provider_->write(data, len);
}

}
