#pragma once
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "ndx/acquisition_backend.hpp"

namespace ndx {

struct CharCallback {
  std::string char_uuid;
  std::optional<std::string> char_name;
  std::function<void(const Packet&)> on_data;
};

using CharCallbacks = std::vector<CharCallback>;

}
