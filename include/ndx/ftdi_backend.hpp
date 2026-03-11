#pragma once
#include "acquisition_backend.hpp"
#include <string>

namespace ndx {

class FtdiBackend : public AcquisitionBackend {
public:
  explicit FtdiBackend(const std::string& device_id);
  void start(PacketCallback cb) override;
  std::string name() const override { return "FtdiBackend"; }
};

}