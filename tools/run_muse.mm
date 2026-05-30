#include "ndx/ndx_ffi.hpp"
#import <Foundation/Foundation.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static const char* MUSE_DEVICE_UUID = "CA6A61B7-B7A8-AF24-3C9E-04A6A5012554";
static const char* CONTROL_CHAR_UUID = "273E0001-4C4D-454D-96BE-F03BAC821358";

static void on_connected() {
  printf("Connected to peripheral\n");
}

static void on_char_data(const uint8_t* data, size_t len, double timestamp_ms) {
  printf("ts=%.0f", timestamp_ms);
  for (size_t i = 0; i < len; i++) printf(" %u", data[i]);
  printf("\n");
}

static void after(double seconds, dispatch_block_t block) {
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(seconds * NSEC_PER_SEC)),
                 dispatch_get_main_queue(), block);
}

static void write_start_commands() {
  const char* commands[] = {"h", "p50", "s", "d"};
  for (const char* cmd : commands) {
    free(write_ble_characteristic(MUSE_DEVICE_UUID, CONTROL_CHAR_UUID, cmd));
  }
}

int main() {
  static const CharCallback callbacks[] = {
    {"273E0003-4C4D-454D-96BE-F03BAC821358", "EEG_TP9",  on_char_data},
    {"273E0004-4C4D-454D-96BE-F03BAC821358", "EEG_AF7",  on_char_data},
    {"273E0005-4C4D-454D-96BE-F03BAC821358", "EEG_AF8",  on_char_data},
    {"273E0006-4C4D-454D-96BE-F03BAC821358", "EEG_TP10", on_char_data},
    {"273E0007-4C4D-454D-96BE-F03BAC821358", "EEG_AUX",  on_char_data},
    {"273E000F-4C4D-454D-96BE-F03BAC821358", "PPG_AMBIENT",  on_char_data},
    {"273E0010-4C4D-454D-96BE-F03BAC821358", "PPG_INFRARED", on_char_data},
    {"273E0011-4C4D-454D-96BE-F03BAC821358", "PPG_RED",      on_char_data},
  };

  free(create_ble_backend("{\"uuid\":\"CA6A61B7-B7A8-AF24-3C9E-04A6A5012554\"}"));
  free(start_ble_backend(MUSE_DEVICE_UUID, on_connected, callbacks, sizeof(callbacks) / sizeof(callbacks[0])));

  after(2.0, ^{ write_start_commands(); });
  after(4.0, ^{ free(write_ble_characteristic(MUSE_DEVICE_UUID, CONTROL_CHAR_UUID, "h")); });
  after(6.0, ^{ free(stop_ble_backend(MUSE_DEVICE_UUID)); exit(0); });

  [[NSRunLoop currentRunLoop] run];
  return 0;
}
