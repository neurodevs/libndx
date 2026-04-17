#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ndx/acquisition_backend.hpp"

namespace ndx {

struct PeripheralInfo {
  std::string uuid;
  std::string name;
};

using ScanResultCallback = std::function<void(const std::vector<PeripheralInfo>&)>;

class BleProvider {
public:
  virtual ~BleProvider() = default;
  virtual bool isPoweredOn() = 0;
  virtual void scanForPeripheral(const std::string& uuid, OnDataCallback on_data) = 0;
  virtual void scanAll(int duration_ms, ScanResultCallback on_complete) = 0;
  virtual int readRssi() = 0;
};

std::unique_ptr<BleProvider> createBleProvider();

}
