#pragma once
#include <functional>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ndx {

struct Packet {
  std::vector<uint32_t> data;
  std::uint64_t timestamp_ms;
};

struct CharCallbackEntry {
  std::string char_uuid;
  std::optional<std::string> char_name;
  std::function<void(const Packet&)> on_data;
};

using CharCallbacks = std::vector<CharCallbackEntry>;

struct Peripheral {
  std::string uuid;
  std::string name;
};

using OnConnectedCallback = std::function<void(const Peripheral*)>;


class AcquisitionBackend {
public:
  explicit AcquisitionBackend(const std::string& device_id);
  virtual ~AcquisitionBackend() = default;
  virtual void start(CharCallbacks callbacks, OnConnectedCallback on_connected = nullptr);
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