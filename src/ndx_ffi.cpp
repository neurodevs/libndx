#include <cstring>
#include <string>
#include <unordered_map>
#include <memory>
#include "ndx/ndx_ffi.hpp"

extern "C" char* createBleBackend(const char* device_id) {
    return strdup("{\"status\": 200 }");
}

