#include "ndx/acquisition_backend.hpp"

namespace ndx {

AcquisitionBackend::AcquisitionBackend(const std::string& device_id)
  : device_id_(device_id) {}

void AcquisitionBackend::start(std::function<void(const Packet&)> callback) {
  if (is_running_) {
    throw std::runtime_error(name() + ": start called while already running");
  }
  callback_ = callback;
  is_running_ = true;
}

void AcquisitionBackend::fire_callback(const Packet& p) {
  callback_(p);
}

void AcquisitionBackend::stop() {
  if (!is_running_) {
    throw std::runtime_error(name() + ": stop called while not running");
  }
  is_running_ = false;
}

void AcquisitionBackend::destroy() {
  intentional_disconnect_ = true;
  
  if (is_running_) {
    stop();
  }
}

}