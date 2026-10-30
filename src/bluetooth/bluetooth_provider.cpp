#include "ndx/bluetooth_provider.hpp"

#include <algorithm>

namespace ndx {

std::vector<std::string> list_bluetooth_addresses(const std::string& name_prefix) {
  std::vector<std::string> addresses;

  for (const auto& device : BluetoothProviderSyscalls::list_paired_devices()) {
    if (device.name.rfind(name_prefix, 0) == 0) {
      addresses.push_back(device.id);
    }
  }

  std::sort(addresses.begin(), addresses.end());
  return addresses;
}

}
