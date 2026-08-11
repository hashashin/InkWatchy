#pragma once

#include "defines.h"

#if BLE_ENABLED || BLE_HOST_ENABLED
extern volatile bool bleClientConnected;
esp_power_level_t getBlePower();
#endif

#if BLE_ENABLED

// Server functions (X chooses to connect to inkwatchy)
extern BLEServer *pServer;
extern BLEService *bleService;

void initBle();
void startBle();
void exitBle();
void cleanupBleDevice(); // For use with other ble implementation that don't use exitBle
#endif

#if BLE_HOST_ENABLED
// Client (Host) functions (Inkwatchy chooses to connect to X)
extern BLEScan *pBLEScan;
void hostBleDeInitEverything();
void hostBleInitClient();
void hostBleStartScan(uint32_t durationSeconds);
void hostBleStartScanAsync(uint32_t durationSeconds, void (*scanCompleteCB)(BLEScanResults));
int hostBleGetScannedDevicesCount();
String hostBleGetScannedDeviceName(int index);
bool hostBleConnectToDevice(int index);
extern String hostBleClientName;
extern notify_callback hostBleNotifyCallback;
#endif
