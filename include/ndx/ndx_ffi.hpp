#pragma once

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
#endif