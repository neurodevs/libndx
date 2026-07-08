// Discovers and connects to an OpenBCI Cyton board over its FTDI USB-serial
// dongle. The Cyton's dongle enumerates as a standard FTDI serial device
// (/dev/cu.usbserial-* on macOS) at 115200 baud. We identify the board by
// sending the soft-reset command ('v') and waiting for the '$$$' terminator
// that the Cyton firmware appends to every response.
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <dirent.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

namespace {

constexpr speed_t kCytonBaud = B115200;
constexpr double kResponseTimeoutSec = 10.0;
constexpr const char* kResetCommand = "v";
constexpr const char* kResponseTerminator = "$$$";
// Opening the port asserts DTR, which resets the dongle's onboard radio MCU
// (same auto-reset circuit as an Arduino). It must finish booting before it
// will respond to any command, or the probe byte is silently dropped.
constexpr double kBootDelaySec = 2.0;

std::vector<std::string> find_candidate_ports() {
  std::vector<std::string> ports;
  const char* dir_path = "/dev";
  DIR* dir = opendir(dir_path);
  if (!dir) {
    perror("opendir(/dev)");
    return ports;
  }
  while (struct dirent* entry = readdir(dir)) {
    std::string name(entry->d_name);
    if (name.rfind("cu.usbserial", 0) == 0 || name.rfind("cu.usbmodem", 0) == 0) {
      ports.push_back(std::string(dir_path) + "/" + name);
    }
  }
  closedir(dir);
  return ports;
}

int open_serial_port(const std::string& path) {
  int fd = open(path.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fd < 0) {
    return -1;
  }

  termios tty{};
  if (tcgetattr(fd, &tty) != 0) {
    close(fd);
    return -1;
  }

  cfmakeraw(&tty);
  cfsetispeed(&tty, kCytonBaud);
  cfsetospeed(&tty, kCytonBaud);

  tty.c_cflag |= (CLOCAL | CREAD);
  tty.c_cflag &= ~PARENB;
  tty.c_cflag &= ~CSTOPB;
  tty.c_cflag &= ~CSIZE;
  tty.c_cflag |= CS8;
  tty.c_cflag &= ~CRTSCTS;

  tty.c_cc[VMIN] = 0;
  tty.c_cc[VTIME] = 0;

  if (tcsetattr(fd, TCSANOW, &tty) != 0) {
    close(fd);
    return -1;
  }

  tcflush(fd, TCIOFLUSH);
  return fd;
}

// Sends the soft-reset command and reads until '$$$' or timeout. Returns
// true if the response looks like it came from a Cyton board.
bool probe_for_cyton(int fd, std::string* response_out) {
  if (write(fd, kResetCommand, std::strlen(kResetCommand)) < 0) {
    return false;
  }

  std::string response;
  const double poll_interval_sec = 0.05;
  const int max_polls = static_cast<int>(kResponseTimeoutSec / poll_interval_sec);

  char buf[256];
  for (int i = 0; i < max_polls; i++) {
    ssize_t n = read(fd, buf, sizeof(buf));
    if (n > 0) {
      response.append(buf, static_cast<size_t>(n));
      if (response.find(kResponseTerminator) != std::string::npos) {
        *response_out = response;
        return true;
      }
    } else if (n < 0 && errno != EAGAIN) {
      return false;
    }
    usleep(static_cast<useconds_t>(poll_interval_sec * 1'000'000));
  }
  return false;
}

}  // namespace

int main() {
  std::vector<std::string> ports = find_candidate_ports();
  if (ports.empty()) {
    fprintf(stderr, "No candidate serial ports found under /dev (cu.usbserial-*, cu.usbmodem-*).\n");
    return 1;
  }

  printf("Found %zu candidate port(s):\n", ports.size());
  for (const auto& port : ports) {
    printf("  %s\n", port.c_str());
  }

  for (const auto& port : ports) {
    printf("\nProbing %s...\n", port.c_str());
    int fd = open_serial_port(port);
    if (fd < 0) {
      printf("  Could not open (%s)\n", strerror(errno));
      continue;
    }

    usleep(static_cast<useconds_t>(kBootDelaySec * 1'000'000));
    tcflush(fd, TCIOFLUSH);  // discard boot chatter before probing

    std::string response;
    bool is_cyton = probe_for_cyton(fd, &response);
    if (is_cyton) {
      printf("  Cyton board found on %s\n", port.c_str());
      printf("  Response: %s\n", response.c_str());
      printf("\nConnected. fd=%d, port=%s, baud=115200\n", fd, port.c_str());
      printf("Leave this process running to keep the connection open. Ctrl-C to exit.\n");

      // Keep reading and echoing any subsequent output (e.g. once streaming
      // is started with 'b') so the connection stays alive for inspection.
      char buf[256];
      while (true) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n > 0) {
          fwrite(buf, 1, static_cast<size_t>(n), stdout);
          fflush(stdout);
        }
        usleep(20'000);
      }
    }

    printf("  Not a Cyton (no response within %.1fs)\n", kResponseTimeoutSec);
    close(fd);
  }

  fprintf(stderr, "\nNo OpenBCI Cyton board found on any candidate port.\n");
  return 1;
}
