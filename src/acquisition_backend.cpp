#include "ndx/acquisition_backend.hpp"

namespace ndx {

AcquisitionBackend::AcquisitionBackend(const std::string& device_id)
  : device_id_(device_id) {}

}