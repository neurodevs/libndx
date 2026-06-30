#pragma once
#include <memory>
#include <string>

#include "acquisition_backend.hpp"
#include "ftdi_provider.hpp"

namespace ndx {

class FtdiBackend : public AcquisitionBackend {
public:
  explicit FtdiBackend(const std::string& device_id, std::unique_ptr<FtdiProvider> provider);
  virtual void start(OnDataCallback on_data, OnConnectedCallback on_connected = nullptr);
  void stop() override;
  std::string name() const override { return "FtdiBackend"; }

private:
  std::unique_ptr<FtdiProvider> provider_;
};

}
