#include "ndx/ndx_ffi.hpp"
#import <Foundation/Foundation.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static const char* MUSE_DEVICE_UUID = "CA6A61B7-B7A8-AF24-3C9E-04A6A5012554";
static const char* CONTROL_CHAR_UUID = "273E0001-4C4D-454D-96BE-F03BAC821358";

static void on_data(const uint32_t* data, size_t len, double timestamp_ms) {
  printf("ts=%.0f", timestamp_ms);
  for (size_t i = 0; i < len; i++) printf(" %u", data[i]);
  printf("\n");
}

static void write_start_commands() {
  const char* commands[] = {"h", "p50", "s", "d"};
  for (const char* cmd : commands) {
    char buf[32];
    size_t cmd_len = strlen(cmd);
    buf[0] = (char)(cmd_len + 1);
    memcpy(buf + 1, cmd, cmd_len);
    buf[1 + cmd_len] = '\n';
    free(write_ble_characteristic(MUSE_DEVICE_UUID, CONTROL_CHAR_UUID, buf));
  }
}

int main() {
  free(create_ble_backend("{\"uuid\":\"CA6A61B7-B7A8-AF24-3C9E-04A6A5012554\"}"));
  free(start_ble_backend(MUSE_DEVICE_UUID, on_data));

  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(2.0 * NSEC_PER_SEC)),
                 dispatch_get_main_queue(), ^{ write_start_commands(); });

  [[NSRunLoop currentRunLoop] run];
  return 0;
}
