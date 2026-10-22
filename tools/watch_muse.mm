#include "ndx/ndx_ffi.hpp"
#import <Foundation/Foundation.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

// Streams from the first Muse found and reports, once per second, how many
// packets arrived. Turn the headband off to see how soon the lost connection
// is reported, and how long after the last packet. Turn it back on to see it
// reconnect and resume streaming.

static const char* CONTROL_CHAR_UUID = "273E0001-4C4D-454D-96BE-F03BAC821358";
static const double RUN_SECONDS = 90.0;

static char* discovered_uuid = nullptr;
static std::atomic<int> packets_this_second{0};
static std::atomic<double> last_packet_sec{0};
static double started_sec = 0;

static double now_sec() {
  return [NSDate timeIntervalSinceReferenceDate];
}

static double elapsed() {
  return now_sec() - started_sec;
}

static void write_start_commands();
static void report_each_second();
static void after(double seconds, dispatch_block_t block);

static void on_connected(const char* uuid, const char* name) {
  printf("[%6.2fs] CONNECTED to %s (%s)\n", elapsed(), name ? name : "", uuid ? uuid : "");
  fflush(stdout);
  after(3.0, ^{
    write_start_commands();
    report_each_second();
  });
}

static void on_connection_status(const char* status) {
  if (strcmp(status, "lost") == 0) {
    double since_packet = now_sec() - last_packet_sec.load();
    printf("[%6.2fs] LOST connection, %.2fs after the last packet\n", elapsed(), since_packet);
  } else {
    printf("[%6.2fs] RECONNECTED\n", elapsed());
    write_start_commands();
  }
  fflush(stdout);
}

static void on_char_data(const uint8_t*, size_t, double) {
  packets_this_second++;
  last_packet_sec = now_sec();
}

static const CharCallback callbacks[5] = {
  {"273E0003-4C4D-454D-96BE-F03BAC821358", "EEG_TP9",  on_char_data},
  {"273E0004-4C4D-454D-96BE-F03BAC821358", "EEG_AF7",  on_char_data},
  {"273E0005-4C4D-454D-96BE-F03BAC821358", "EEG_AF8",  on_char_data},
  {"273E0006-4C4D-454D-96BE-F03BAC821358", "EEG_TP10", on_char_data},
  {"273E0007-4C4D-454D-96BE-F03BAC821358", "EEG_AUX",  on_char_data},
};

static void after(double seconds, dispatch_block_t block) {
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(seconds * NSEC_PER_SEC)),
                 dispatch_get_main_queue(), block);
}

static bool has_said_to_turn_off = false;

static void report_each_second() {
  after(1.0, ^{
    int packets = packets_this_second.exchange(0);
    printf("[%6.2fs] %d packets\n", elapsed(), packets);
    if (packets > 0 && !has_said_to_turn_off) {
      has_said_to_turn_off = true;
      printf("[%6.2fs] >>> Streaming. Turn the Muse off now. <<<\n", elapsed());
    }
    fflush(stdout);
    report_each_second();
  });
}

static void write_start_commands() {
  const char* commands[] = {"h", "p21", "s", "d"};
  for (const char* cmd : commands) {
    free(write_ble_gatt_char(discovered_uuid, CONTROL_CHAR_UUID, cmd));
  }
}

static void on_discovered(const char* uuid) {
  discovered_uuid = strdup(uuid);
  printf("[%6.2fs] Discovered Muse %s\n", elapsed(), discovered_uuid);
  fflush(stdout);
}

int main() {
  started_sec = now_sec();
  free(discover_ble_uuid("Muse", on_discovered));

  after(5.0, ^{
    if (!discovered_uuid) { printf("No Muse found. Is it on?\n"); exit(1); }
    std::string config = std::string("{\"uuid\":\"") + discovered_uuid + "\"}";
    free(create_ble_gatt_backend(config.c_str()));
    free(start_ble_gatt_backend(discovered_uuid, on_connected, on_connection_status, callbacks,
                                sizeof(callbacks) / sizeof(callbacks[0])));
  });
  after(RUN_SECONDS, ^{ free(stop_ble_gatt_backend(discovered_uuid)); exit(0); });

  [[NSRunLoop currentRunLoop] run];
  return 0;
}
