#pragma once
#include "acquisition_backend.hpp"
#include <string>

namespace ndx {

class BleBackend : public AcquisitionBackend {
public:
  explicit BleBackend(const std::string& device_id);
  std::string name() const override { return "BleBackend"; }
};

}