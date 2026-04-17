#pragma once

#include <memory>
#include <functional>
#include <string>

#include "ndx/ble_backend.hpp"
#include "ndx/ftdi_backend.hpp"

extern "C" {

char* createBleBackend(const char* device_uuid);
char* startBleBackend(const char* device_uuid, void (*on_data)(const char* packet_json));
char* writeBleCharacteristic(const char* device_uuid, const char* char_uuid, const char* value);
char* readBleRssi(const char* device_uuid);
char* stopBleBackend(const char* device_uuid);
char* destroyBleBackend(const char* device_uuid);

char* createFtdiBackend(const char* serial_number);
char* startFtdiBackend(const char* serial_number, void (*on_data)(const char* packet_json));
char* stopFtdiBackend(const char* serial_number);
char* destroyFtdiBackend(const char* serial_number);

}

using BleFactory = std::function<std::shared_ptr<ndx::BleBackend>(const std::string&)>;
using FtdiFactory = std::function<std::shared_ptr<ndx::FtdiBackend>(const std::string&)>;

#ifdef NDX_TESTING
std::shared_ptr<ndx::BleBackend> getBleBackend(const std::string& device_uuid);
std::shared_ptr<ndx::FtdiBackend> getFtdiBackend(const std::string& serial_number);
void resetBleBackends();
void resetFtdiBackends();
void setBleFactory(BleFactory factory);
void setFtdiFactory(FtdiFactory factory);
#endif
