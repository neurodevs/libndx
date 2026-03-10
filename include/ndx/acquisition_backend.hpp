#pragma once
#include <functional>
#include <cstddef>
#include <cstdint>

namespace ndx {

struct Packet;
using PacketCallback = std::function<void(const Packet&)>;

class AcquisitionBackend {
public:
  explicit AcquisitionBackend(const std::string& device_id);
  virtual ~AcquisitionBackend() = default;
  virtual void start(PacketCallback cb) = 0;
  virtual void stop() = 0;
  virtual bool is_running() const { return is_running_; }

protected:
  std::string device_id_;
  bool is_running_ = false;
};

}