// Discovers and connects to an OpenBCI Cyton board over its FTDI USB-serial
// dongle, using the ndx UsbProvider. The Cyton's dongle enumerates as a
// standard FTDI serial device (/dev/cu.usbserial-* on macOS) at 115200 baud.
// We identify the board by sending the soft-reset command ('v') and waiting
// for the '$$$' terminator that the Cyton firmware appends to every response.
#include "ndx/usb_provider.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <dirent.h>

namespace {

// Opening the port asserts DTR, which resets the dongle's onboard radio MCU
// (same auto-reset circuit as an Arduino). It must finish booting before it
// will respond to any command, or the probe byte is silently dropped.
constexpr int kBootDelayMs = 2000;
constexpr double kResponseTimeoutSec = 10.0;
constexpr const char* kResetCommand = "v";
constexpr const char* kResponseTerminator = "$$$";

// The device_id ndx::UsbProvider expects is the serial number suffix of
// /dev/cu.usbserial-<serial>, so candidates are collected as bare serials.
std::vector<std::string> find_candidate_serials() {
  std::vector<std::string> serials;
  const char* dir_path = "/dev";
  const std::string prefix = "cu.usbserial-";
  DIR* dir = opendir(dir_path);
  if (!dir) {
    perror("opendir(/dev)");
    return serials;
  }
  while (struct dirent* entry = readdir(dir)) {
    std::string name(entry->d_name);
    if (name.rfind(prefix, 0) == 0) {
      serials.push_back(name.substr(prefix.size()));
    }
  }
  closedir(dir);
  return serials;
}

}  // namespace

int main() {
  std::vector<std::string> serials = find_candidate_serials();
  if (serials.empty()) {
    fprintf(stderr, "No candidate serial devices found under /dev (cu.usbserial-*).\n");
    return 1;
  }

  printf("Found %zu candidate device(s):\n", serials.size());
  for (const auto& serial : serials) {
    printf("  %s\n", serial.c_str());
  }

  for (const auto& serial : serials) {
    printf("\nProbing %s...\n", serial.c_str());

    auto provider = ndx::create_usb_provider();
    std::mutex response_mutex;
    std::string response;
    std::atomic<bool> confirmed{false};

    try {
      provider->connect(
          serial,
          [&](const ndx::Packet& p) {
            if (confirmed) {
              fwrite(p.data.data(), 1, p.data.size(), stdout);
              fflush(stdout);
              return;
            }
            std::lock_guard<std::mutex> lock(response_mutex);
            response.append(p.data.begin(), p.data.end());
          },
          nullptr, kBootDelayMs);
    } catch (const std::exception& e) {
      printf("  Could not open (%s)\n", e.what());
      continue;
    }

    if (!provider->write(reinterpret_cast<const uint8_t*>(kResetCommand), strlen(kResetCommand))) {
      printf("  Could not write reset command\n");
      provider->disconnect();
      continue;
    }

    bool is_cyton = false;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::duration<double>(kResponseTimeoutSec);
    while (std::chrono::steady_clock::now() < deadline) {
      {
        std::lock_guard<std::mutex> lock(response_mutex);
        if (response.find(kResponseTerminator) != std::string::npos) {
          is_cyton = true;
          break;
        }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    if (is_cyton) {
      printf("  Cyton board found on %s\n", serial.c_str());
      {
        std::lock_guard<std::mutex> lock(response_mutex);
        printf("  Response: %s\n", response.c_str());
      }
      printf("\nConnected. serial=%s, baud=115200\n", serial.c_str());
      printf("Leave this process running to keep the connection open. Ctrl-C to exit.\n");

      confirmed = true;
      while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
      }
    }

    printf("  Not a Cyton (no response within %.1fs)\n", kResponseTimeoutSec);
    provider->disconnect();
  }

  fprintf(stderr, "\nNo OpenBCI Cyton board found on any candidate device.\n");
  return 1;
}
