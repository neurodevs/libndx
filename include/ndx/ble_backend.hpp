#pragma once
#include "acquisition_backend.hpp"

namespace ndx {

class BleBackend : public AcquisitionBackend {
public:
  void start(PacketCallback cb) override {}
  void stop() override {}
};

}