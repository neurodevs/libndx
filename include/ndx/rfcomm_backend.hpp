#pragma once
#include "acquisition_backend.hpp"
#include "rfcomm_provider.hpp"
#include <string>

namespace ndx {

class RfcommBackend : public AcquisitionBackend {
public:
  explicit RfcommBackend(const std::string& device_address, int port,
                            std::unique_ptr<RfcommProvider> provider);
  virtual void start(OnDataCallback on_data, OnConnectedCallback on_connected = nullptr);
  virtual bool write(const uint8_t* data, size_t len);
  void stop() override;
  std::string name() const override { return "RfcommBackend"; }

private:
  int port_;
  std::unique_ptr<RfcommProvider> provider_;
};

}
