#pragma once
#include <functional>
#include <cstddef>
#include <cstdint>

namespace ndx {

struct Packet;
using PacketCallback = std::function<void(const Packet&)>;

class AcquisitionBackend {
public:
  virtual ~AcquisitionBackend() = default;
  virtual void start(PacketCallback cb) = 0;
  virtual void stop() = 0;
  virtual bool is_running() const = 0;
};

}