#include "ndx/ftdi_backend.hpp"

namespace ndx {

FtdiBackend::FtdiBackend(const std::string& device_id)
  : AcquisitionBackend(device_id) {}

void FtdiBackend::start(std::function<void(const Packet&)> callback) {
  is_running_ = true;
}

}