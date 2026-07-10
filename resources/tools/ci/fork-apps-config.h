#pragma once

// Public CI profile. It starts from the normal template and enables the
// fork-specific apps that should stay buildable without personal settings.
#include "templates/gifnoc-template.h"

#undef BLE_ENABLED
#define BLE_ENABLED 1

#undef GOTCHI
#define GOTCHI 1

#undef QR_APP
#define QR_APP 1

#undef RSS_READER
#define RSS_READER 1

#undef HA_CONTROL
#define HA_CONTROL 1

#undef CHESS
#define CHESS 1

#undef FS_UPLOAD
#define FS_UPLOAD 1

#undef PRESENCE_BEACON
#define PRESENCE_BEACON 1

#undef STOPWATCH
#define STOPWATCH 1

#undef MOON_SUN_APP
#define MOON_SUN_APP 1

#undef SUBNET_CALC_APP
#define SUBNET_CALC_APP 1

#undef WORLD_CLOCK_APP
#define WORLD_CLOCK_APP 1

#undef PINOUT_WALLET_APP
#define PINOUT_WALLET_APP 1

#undef RTC_DRIFT_LAB_APP
#define RTC_DRIFT_LAB_APP 1

#undef ISS_PASSES_APP
#define ISS_PASSES_APP 1

#undef APPLE_JOKE
#define APPLE_JOKE 1
