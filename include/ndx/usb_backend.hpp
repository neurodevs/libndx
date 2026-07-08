#pragma once
#include <memory>
#include <string>

#include "acquisition_backend.hpp"
#include "usb_provider.hpp"

namespace ndx {

class UsbBackend : public AcquisitionBackend {
public:
  explicit UsbBackend(const std::string& device_id, std::unique_ptr<UsbProvider> provider);
  virtual void start(OnDataCallback on_data, OnConnectedCallback on_connected = nullptr);
  void stop() override;
  std::string name() const override { return "UsbBackend"; }

private:
  std::unique_ptr<UsbProvider> provider_;
};

}
