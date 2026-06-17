#pragma once
#include "acquisition_backend.hpp"
#include "bluetooth_provider.hpp"
#include <string>

namespace ndx {

class BluetoothBackend : public AcquisitionBackend {
public:
  explicit BluetoothBackend(const std::string& device_address, int port,
                            std::unique_ptr<BluetoothProvider> provider);
  void start(OnDataCallback on_data, OnConnectedCallback on_connected = nullptr);
  void stop() override;
  std::string name() const override { return "BluetoothBackend"; }

private:
  int port_;
  std::unique_ptr<BluetoothProvider> provider_;
};

}
