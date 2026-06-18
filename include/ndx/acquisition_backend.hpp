#pragma once
#include <functional>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ndx {

struct Packet {
  std::vector<uint8_t> data;
  double timestamp_sec;
};

struct Device {
  std::string id;
  std::string name;
};

using OnConnectedCallback = std::function<void(const Device*)>;
using OnDataCallback = std::function<void(const Packet&)>;


class AcquisitionBackend {
public:
  explicit AcquisitionBackend(const std::string& device_id);
  virtual ~AcquisitionBackend() = default;
  virtual void start();
  virtual void stop();
  bool is_running() const { return is_running_; }
  const std::string& device_id() const { return device_id_; }
  std::string device_id_;

protected:
  virtual std::string name() const = 0;
  bool is_intentional_disconnect() const { return intentional_disconnect_; }

private:
  bool is_running_ = false;
  bool intentional_disconnect_ = false;
};

}