#include "ndx/ftdi_backend.hpp"

namespace ndx {

FtdiBackend::FtdiBackend(const std::string& device_id)
  : AcquisitionBackend(device_id) {}

void FtdiBackend::start(PacketCallback cb) {
  AcquisitionBackend::start(cb);
  callback_ = cb;
}

void FtdiBackend::fireCallback(const Packet& p) {
  callback_(p);
}

}
