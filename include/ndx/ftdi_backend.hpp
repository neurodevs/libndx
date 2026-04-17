#pragma once
#include <string>

#include "ndx/acquisition_backend.hpp"

namespace ndx {

class FtdiBackend : public AcquisitionBackend {
public:
  explicit FtdiBackend(const std::string& device_id);
  std::string name() const override { return "FtdiBackend"; }
};

}