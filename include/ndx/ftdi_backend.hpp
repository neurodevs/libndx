#pragma once
#include "acquisition_backend.hpp"
#include <string>

namespace ndx {

class FtdiBackend : public AcquisitionBackend {
public:
  explicit FtdiBackend(const std::string& device_id);
  std::string name() const override { return "FtdiBackend"; }
};

}