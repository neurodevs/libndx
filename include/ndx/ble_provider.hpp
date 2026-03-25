#pragma once
#include "acquisition_backend.hpp"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ndx {

struct PeripheralInfo {
  std::string id;
  std::string name;
};

using ScanResultCallback = std::function<void(const std::vector<PeripheralInfo>&)>;

class BleProvider {
public:
  virtual ~BleProvider() = default;
  virtual bool isPoweredOn() = 0;
  virtual void scanForPeripheral(const std::string& id, OnDataCallback on_data) = 0;
  virtual void scanAll(int duration_ms, ScanResultCallback on_complete) = 0;
};

std::unique_ptr<BleProvider> createBleProvider();

}
