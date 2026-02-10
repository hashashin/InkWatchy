#pragma once

#include "defines.h"

// Background iBeacon presence service (always-on).
// Ticked from the main/manager loop so it runs in any UI place.

#if PRESENCE_BEACON

void presenceBeaconSvcInit();
void presenceBeaconSvcTick();
void presenceBeaconSvcStop();

bool presenceBeaconSvcGetEnabled();
void presenceBeaconSvcSetEnabled(bool on);

uint32_t presenceBeaconSvcGetPeriodMs();
void presenceBeaconSvcSetPeriodMs(uint32_t ms);

uint32_t presenceBeaconSvcGetBurstMs();
void presenceBeaconSvcSetBurstMs(uint32_t ms);

int presenceBeaconSvcGetTxPowerIndex();
void presenceBeaconSvcSetTxPowerIndex(int idx);

const char* presenceBeaconSvcGetUuid();
uint16_t presenceBeaconSvcGetMajor();
uint16_t presenceBeaconSvcGetMinor();

// Forces the service to restart advertising with current config
void presenceBeaconSvcRequestRestart();

#endif
