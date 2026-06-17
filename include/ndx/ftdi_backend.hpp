#pragma once
#include <string>

#include "ndx/acquisition_backend.hpp"

namespace ndx {

class FtdiBackend : public AcquisitionBackend {
public:
  explicit FtdiBackend(const std::string& device_id);
  virtual void start(OnDataCallback on_data, OnConnectedCallback on_connected = nullptr);
  std::string name() const override { return "FtdiBackend"; }
};

}