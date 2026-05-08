#include "ndx/ble_backend.hpp"
#include "ndx/ble_provider.hpp"
#import <Foundation/Foundation.h>
#include <cstdio>

static const char* MUSE_DEVICE_UUID = "CA6A61B7-B7A8-AF24-3C9E-04A6A5012554";

int main() {
  ndx::BleBackend backend(MUSE_DEVICE_UUID, ndx::create_ble_provider());

  backend.start([](const ndx::Packet& p) {
    printf("t=%llu  words=%zu\n",
           (unsigned long long)p.timestamp_ms,
           p.data.size());
  });

  [[NSRunLoop currentRunLoop] run];
  return 0;
}
