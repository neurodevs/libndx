#include "ndx/ble_provider.hpp"

namespace ndx {

class BleProviderImpl : public BleProvider {
public:
  bool is_powered_on() override { return false; }
  void scan_for_peripheral(const std::string&, ndx::OnDataCallback) override {}
};

std::unique_ptr<BleProvider> create_ble_provider() {
  return std::make_unique<BleProviderImpl>();
}

}
