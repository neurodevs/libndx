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
  virtual void start(PacketCallback cb);
  virtual void stop();
  virtual bool is_running() const { return is_running_; }

protected:
  std::string device_id_;
  bool is_running_ = false;
  virtual std::string name() const = 0;
};

}