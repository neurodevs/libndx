#include "ndx/ble_backend.hpp"
#include <stdexcept>

namespace ndx {

BleBackend::BleBackend(const std::string& device_id)
  : AcquisitionBackend(device_id) {}

void BleBackend::start(PacketCallback cb) {
  if (!isBluetoothPoweredOn()) {
    throw std::runtime_error("BleBackend: Bluetooth is not powered on");
  }
  AcquisitionBackend::start(cb);
}

bool BleBackend::isBluetoothPoweredOn() {
  return true;
}

}