#include "ndx/usb_provider.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

#include <dirent.h>

namespace {

std::string find_serial() {
  const std::string prefix = "cu.usbserial-";
  DIR* dir = opendir("/dev");
  if (!dir) return "";
  std::string serial;
  while (struct dirent* entry = readdir(dir)) {
    std::string name(entry->d_name);
    if (name.rfind(prefix, 0) == 0) {
      serial = name.substr(prefix.size());
      break;
    }
  }
  closedir(dir);
  return serial;
}

}

int main() {
  std::string serial = find_serial();
  if (serial.empty()) {
    fprintf(stderr, "✗ No cu.usbserial-* device found under /dev.\n");
    return 1;
  }

  printf("→ Found device %s\n", serial.c_str());
  printf("→ Connecting (waiting for the dongle to boot)...\n");

  auto provider = ndx::create_usb_provider();
  provider->connect(
      serial,
      [](const ndx::Packet& p) {
        fwrite(p.data.data(), 1, p.data.size(), stdout);
        fflush(stdout);
      },
      nullptr, 2000);

  printf("✓ Connected. Sending reset command...\n");
  printf("----------------------------------------\n");

  const char* reset_command = "v";
  provider->write(reinterpret_cast<const uint8_t*>(reset_command), strlen(reset_command));

  while (true) std::this_thread::sleep_for(std::chrono::seconds(1));
}
