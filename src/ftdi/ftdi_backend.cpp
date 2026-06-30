#include "ndx/ftdi_backend.hpp"
#include <stdexcept>

namespace ndx {

FtdiBackend::FtdiBackend(const std::string& device_id, std::unique_ptr<FtdiProvider> provider)
  : AcquisitionBackend(device_id), provider_(std::move(provider)) {}

void FtdiBackend::start(OnDataCallback on_data, OnConnectedCallback on_connected) {
  AcquisitionBackend::start();

  provider_->connect(device_id_, std::move(on_data), std::move(on_connected));
}

void FtdiBackend::stop() {
  AcquisitionBackend::stop();
  provider_->disconnect();
}

}
