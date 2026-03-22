#include "ndx/ble_backend.hpp"

namespace ndx {

BleBackend::BleBackend(const std::string& device_id)
  : AcquisitionBackend(device_id) {}

void BleBackend::start(PacketCallback cb) {
  AcquisitionBackend::start(cb);
  callback_ = cb;
}

void BleBackend::fireCallback(const Packet& p) {
  callback_(p);
}

}