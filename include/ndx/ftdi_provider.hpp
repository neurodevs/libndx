#pragma once
#include "acquisition_backend.hpp"
#include <memory>
#include <string>

namespace ndx {

class FtdiProvider {
public:
  virtual ~FtdiProvider() = default;
  virtual bool is_powered_on() = 0;
  virtual void connect(const std::string& device_id,
                       OnDataCallback on_data,
                       OnConnectedCallback on_connected) = 0;
  virtual void disconnect() = 0;
};

std::unique_ptr<FtdiProvider> create_ftdi_provider();

}
