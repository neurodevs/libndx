#pragma once
#include "acquisition_backend.hpp"
#include <string>

namespace ndx {

class BleBackend : public AcquisitionBackend {
public:
  explicit BleBackend(const std::string& address);
  void start(PacketCallback cb) override {}
  void stop() override {}

private:
  std::string address_;
};

}