#pragma once
#include <cstdint>
#include <cstddef>
#include <memory>
#include <string>

#include "acquisition_backend.hpp"
#include "usb_provider.hpp"

namespace ndx {

class UsbBackend : public AcquisitionBackend {
public:
  explicit UsbBackend(const std::string& device_id, std::unique_ptr<UsbProvider> provider);
  virtual void start(OnDataCallback on_data, OnConnectedCallback on_connected = nullptr,
                     int wait_after_connect_ms = 0);
  void stop() override;
  virtual bool write(const uint8_t* data, size_t len);
  std::string name() const override { return "UsbBackend"; }

private:
  std::unique_ptr<UsbProvider> provider_;
};

}
