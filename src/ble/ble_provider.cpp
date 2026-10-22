#include "ndx/acquisition_backend.hpp"
#include "ndx/ble_provider.hpp"

namespace ndx {

class BleProviderImpl : public BleProvider {
public:
  bool is_powered_on() override { return false; }
  void scan_for_peripheral(const std::string&, ndx::CharCallbacks,  ndx::OnConnectedCallback,
                           ndx::OnPeripheralDisconnectedCallback) override {}
  void add_char_callbacks(ndx::CharCallbacks) override {}
  void start_advertisement_listen(const std::string&, ndx::OnDataCallback) override {}
  void stop_advertisement_listen() override {}
  void start_ble_scan(const std::string&, std::function<void(const std::string&, const std::string&)>) override {}
  void stop_ble_scan() override {}
};

std::unique_ptr<BleProvider> create_ble_provider() {
  return std::make_unique<BleProviderImpl>();
}

}
