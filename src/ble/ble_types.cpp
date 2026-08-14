#include "ndx/ble_types.hpp"

#include <nlohmann/json.hpp>
#include <iomanip>
#include <sstream>

namespace ndx {

static std::string to_hex(const std::vector<uint8_t>& bytes) {
  std::ostringstream out;
  out << std::hex << std::setfill('0');
  for (uint8_t b : bytes) out << std::setw(2) << static_cast<int>(b);
  return out.str();
}

nlohmann::json Advertisement::to_json() const {
  nlohmann::json service_data_json = nlohmann::json::object();
  for (const auto& sd : service_data) service_data_json[sd.uuid] = to_hex(sd.data);

  return {
    {"localName", local_name},
    {"companyId", company_id ? nlohmann::json(*company_id) : nlohmann::json(nullptr)},
    {"manufacturerData", to_hex(manufacturer_data)},
    {"serviceUuids", service_uuids},
    {"serviceData", service_data_json},
    {"rssi", rssi ? nlohmann::json(*rssi) : nlohmann::json(nullptr)},
    {"txPowerLevel", tx_power_level ? nlohmann::json(*tx_power_level) : nlohmann::json(nullptr)},
    {"isConnectable", is_connectable},
    {"timestampSec", timestamp_sec},
  };
}

}
