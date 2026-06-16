#pragma once
#include "acquisition_backend.hpp"
#include <functional>
#include <string>

namespace ndx {

class BtProvider {
public:
  virtual ~BtProvider() = default;
  virtual bool is_powered_on() = 0;
  virtual void connect(const std::string& address, int port,
                       OnDataCallback on_data,
                       OnConnectedCallback on_connected) = 0;
  virtual void disconnect() = 0;
};

}
