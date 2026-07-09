#include "ndx/usb_provider.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

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

std::atomic<bool> g_should_exit{false};

void handle_sigint(int) { g_should_exit = true; }

constexpr uint8_t kPacketStartByte = 0xA0;
constexpr size_t kPacketSize = 33;
constexpr size_t kNumChannels = 8;

int32_t decode_24bit_signed(const uint8_t* b) {
  int32_t value = (b[0] << 16) | (b[1] << 8) | b[2];
  if (value & 0x800000) value |= 0xFF000000;  // sign-extend
  return value;
}

void print_packet(const uint8_t* packet) {
  printf("Sample %3u:", packet[1]);
  for (size_t ch = 0; ch < kNumChannels; ch++) {
    printf(" ch%zu=%8d", ch + 1, decode_24bit_signed(&packet[2 + ch * 3]));
  }
  printf("\n");
}

class PacketFramer {
public:
  void feed(const uint8_t* data, size_t len) {
    buffer_.insert(buffer_.end(), data, data + len);

    while (true) {
      auto start = std::find(buffer_.begin(), buffer_.end(), kPacketStartByte);
      buffer_.erase(buffer_.begin(), start);

      if (buffer_.size() < kPacketSize) return;

      uint8_t stop_byte = buffer_[kPacketSize - 1];
      if ((stop_byte & 0xF0) != 0xC0) {
        buffer_.erase(buffer_.begin());
        continue;
      }

      print_packet(buffer_.data());
      buffer_.erase(buffer_.begin(), buffer_.begin() + kPacketSize);
    }
  }

private:
  std::vector<uint8_t> buffer_;
};

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
  PacketFramer framer;

  try {
    provider->connect(
        serial,
        [&](const ndx::Packet& p) { framer.feed(p.data.data(), p.data.size()); },
        nullptr, 2000);
  } catch (const std::exception& e) {
    fprintf(stderr, "✗ Could not connect: %s\n", e.what());
    return 1;
  }

  signal(SIGINT, handle_sigint);

  printf("✓ Connected. Sending reset command...\n");
  printf("----------------------------------------\n");

  const char* reset_command = "v";
  provider->write(reinterpret_cast<const uint8_t*>(reset_command), strlen(reset_command));

  std::this_thread::sleep_for(std::chrono::seconds(1));

  printf("→ Starting stream...\n");
  const char* start_stream_command = "b";
  provider->write(reinterpret_cast<const uint8_t*>(start_stream_command), strlen(start_stream_command));

  while (!g_should_exit) std::this_thread::sleep_for(std::chrono::milliseconds(100));

  printf("\n→ Disconnecting...\n");
  provider->disconnect();
  return 0;
}
