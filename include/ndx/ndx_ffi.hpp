#pragma once

#include <memory>
#include "ndx/ble_backend.hpp"
#include "ndx/ftdi_backend.hpp"

#ifdef __cplusplus
extern "C" {
#endif

char* createBleBackend(const char* config_json);
char* startBleBackend(const char* id);
char* stopBleBackend(const char* id);
char* destroyBleBackend(const char* id);

char* createFtdiBackend(const char* config_json);
char* startFtdiBackend(const char* id);
char* stopFtdiBackend(const char* id);
char* destroyFtdiBackend(const char* id);

void ndx_free_string(char* ptr);

#ifdef __cplusplus
}

#include <functional>
#include <string>
using BleFactory = std::function<std::shared_ptr<ndx::BleBackend>(const std::string&)>;
using FtdiFactory = std::function<std::shared_ptr<ndx::FtdiBackend>(const std::string&)>;

#ifdef NDX_TESTING
std::shared_ptr<ndx::BleBackend> getBleBackend(int id);
std::shared_ptr<ndx::FtdiBackend> getFtdiBackend(int id);
void resetBleBackends();
void resetFtdiBackends();
void setBleFactory(BleFactory factory);
void setFtdiFactory(FtdiFactory factory);
#endif

#endif