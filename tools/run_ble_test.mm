#include "ndx/ble_backend.hpp"
#include "ndx/ble_provider.hpp"
#import <Foundation/Foundation.h>
#include <iostream>

int main() {
  const std::string device_id = "CA6A61B7-B7A8-AF24-3C9E-04A6A5012554";

  auto backend = std::make_unique<ndx::BleBackend>(
    device_id,
    ndx::createBleProvider()
  );

  backend->start([](const ndx::Packet& p) {
    std::cout << "packet: " << p.data.size() << " samples @ " << p.timestamp_ms << "ms\n";
  });

  [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:10.0]];

  backend->stop();
  return 0;
}
