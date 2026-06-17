#pragma once
#include "acquisition_backend.hpp"
#include "bt_provider.hpp"
#include <string>

namespace ndx {

class BtBackend : public AcquisitionBackend {
public:
  explicit BtBackend(const std::string& device_address, int port,
                     std::unique_ptr<BtProvider> provider);
  void start(OnDataCallback on_data, OnConnectedCallback on_connected = nullptr);
  void stop() override;
  std::string name() const override { return "BtBackend"; }

private:
  int port_;
  std::unique_ptr<BtProvider> provider_;
};

}
