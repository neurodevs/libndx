#include "ndx/ble_backend.hpp"

namespace ndx {

BleBackend::BleBackend(const std::string& device_id)
  : AcquisitionBackend(device_id) {}

}