#include "ndx/ble_observer_backend.hpp"
#include "ndx/ble_provider.hpp"
#import <Foundation/Foundation.h>
#include <cstdio>
#include <cstdlib>
#include <memory>

// Same device as tools/run_govee.mm, but driven through BleObserverBackend
// instead of talking to CoreBluetooth directly.
static const char* GOVEE_DEVICE_UUID = "179F4A82-A2DF-C241-DB2A-1DF990779106";

// Govee H5074 manufacturer data layout (little-endian):
//   [0-1] manufacturer ID (0x88, 0xEC)
//   [2]   padding (0x00)
//   [3-4] temperature * 100  (signed int16 LE, °C)
//   [5-6] humidity * 100     (uint16 LE, %)
//   [7]   battery percentage
static void on_advertisement(const ndx::Advertisement& advertisement) {
  if (advertisement.manufacturer_data.size() < 8) return;

  const uint8_t* d = advertisement.manufacturer_data.data();
  // Ignore Apple iBeacon packets (manufacturer ID 0x004C) — the device also
  // broadcasts an INTELLI_ROCKS iBeacon which shares the same peripheral UUID
  if (d[0] != 0x88 || d[1] != 0xEC) return;

  double temp_c   = (int16_t)((uint16_t)d[3] | ((uint16_t)d[4] << 8)) / 100.0;
  double humidity = ((uint16_t)d[5] | ((uint16_t)d[6] << 8)) / 100.0;
  int    battery  = d[7];

  double temp_f = temp_c * 9.0 / 5.0 + 32.0;
  printf("ts=%.6f  temp: %.2f°C / %.2f°F   humidity: %.1f%%   battery: %d%%\n",
         advertisement.timestamp_sec, temp_c, temp_f, humidity, battery);
  fflush(stdout);
}

static void after(double seconds, dispatch_block_t block) {
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(seconds * NSEC_PER_SEC)),
                 dispatch_get_main_queue(), block);
}

// usage: run_govee_observer [seconds]   (0 = run until interrupted, default 0s)
int main(int argc, char** argv) {
  double duration_sec = argc > 1 ? atof(argv[1]) : 0.0;

  static ndx::BleObserverBackend backend(GOVEE_DEVICE_UUID, ndx::create_ble_provider());

  printf("listening for %s advertisements...\n", GOVEE_DEVICE_UUID);
  try {
    backend.start(on_advertisement);
  } catch (const std::exception& e) {
    printf("failed to start: %s\n", e.what());
    return 1;
  }

  if (duration_sec > 0) {
    after(duration_sec, ^{
      backend.stop();
      printf("stopped after %.0fs\n", duration_sec);
      exit(0);
    });
  }

  [[NSRunLoop currentRunLoop] run];
  return 0;
}
