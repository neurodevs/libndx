#pragma once
#include <functional>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <string>

namespace ndx {

struct Packet {
  std::vector<uint32_t> data;
  std::uint64_t timestamp_ms;
};

using OnDataCallback = std::function<void(const Packet&)>;

class AcquisitionBackend {
public:
  explicit AcquisitionBackend(const std::string& device_id);
  virtual ~AcquisitionBackend() = default;
  virtual void start(OnDataCallback cb);
  virtual void stop();
  virtual void destroy();
  bool is_running() const { return is_running_; }
  const std::string& device_id() const { return device_id_; }
  bool isIntentionalDisconnect() const { return false; }


protected:
  std::string device_id_;
  bool is_running_ = false;
  virtual std::string name() const = 0;
  void fireCallback(const Packet& p);

private:
  OnDataCallback callback_;
};

}