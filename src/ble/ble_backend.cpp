#include "ndx/ble_backend.hpp"

namespace ndx {

BleBackend::BleBackend(const std::string& address)
  : address_(address), is_running_(false) {}

  void BleBackend::start(std::function<void(const Packet&)> callback) {
    is_running_ = true;
  }

  bool BleBackend::is_running() const {
    return is_running_;
  }

}