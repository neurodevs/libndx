#include "ndx/ndx_ffi.hpp"
#import <Foundation/Foundation.h>
#include <cstdio>
#include <cstdlib>

static const char* MUSE_DEVICE_UUID = "CA6A61B7-B7A8-AF24-3C9E-04A6A5012554";

static void on_data(const char* packet_json) {
  printf("%s\n", packet_json);
}

int main() {
  free(create_ble_backend(MUSE_DEVICE_UUID));
  free(start_ble_backend(MUSE_DEVICE_UUID, on_data));

  [[NSRunLoop currentRunLoop] run];
  return 0;
}
