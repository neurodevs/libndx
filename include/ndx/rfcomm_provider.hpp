#pragma once
#include "acquisition_backend.hpp"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ndx {

class RfcommProvider {
public:
  virtual ~RfcommProvider() = default;
  virtual bool is_powered_on() = 0;
  virtual void connect(const std::string& address, int port,
                       OnDataCallback on_data,
                       OnConnectedCallback on_connected) = 0;
  virtual void disconnect() = 0;
  virtual bool write(const uint8_t* data, size_t len) = 0;
};

std::unique_ptr<RfcommProvider> create_rfcomm_provider();

std::vector<std::string> list_rfcomm_addresses(const std::string& name_prefix);

// IOBluetooth delivers incoming RFCOMM data on the main thread's run loop only,
// which hosts like node never run. They must call this often from the main
// thread to receive any data.
void pump_rfcomm_events();

class RfcommProviderSyscalls {
public:
  static std::function<std::vector<Device>()> list_paired_devices;
};

}
