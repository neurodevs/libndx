#pragma once
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "ndx/acquisition_backend.hpp"

namespace ndx {

struct CharCallback {
  std::string char_uuid;
  std::optional<std::string> char_name;
  std::function<void(const Packet&)> on_data;
};

using CharCallbacks = std::vector<CharCallback>;

struct ServiceData {
  std::string uuid;
  std::vector<uint8_t> data;
};

struct Advertisement {
  std::string local_name;
  std::optional<uint16_t> company_id;
  std::vector<uint8_t> manufacturer_data;
  std::vector<std::string> service_uuids;
  std::vector<ServiceData> service_data;
  std::optional<int> rssi;
  std::optional<int> tx_power_level;
  bool is_connectable = false;
  double timestamp_sec = 0.0;
};

using OnAdvertisementCallback = std::function<void(const Advertisement&)>;

}
