#pragma once
#include "acquisition_backend.hpp"
#include <string>

namespace ndx {

class BleBackend : public AcquisitionBackend {
public:
  explicit BleBackend(const std::string& address);
  void start(PacketCallback cb) override;
  void stop() override;
  bool is_running() const override;

private:
  std::string address_;
  bool is_running_;
};

}