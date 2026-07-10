#pragma once

// Public CI profile. It starts from the normal template and enables the
// fork-specific apps that should stay buildable without personal settings.
#include "templates/gifnoc-template.h"

// Keep this profile close to the fork firmware instead of enabling every
// upstream app and watchface from the template at the same time.
#undef WATCHFACE_TAYCHRON
#define WATCHFACE_TAYCHRON 0

#undef WATCHFACE_SLATE
#define WATCHFACE_SLATE 0

#undef WATCHFACE_DOMAIN_DOTP
#define WATCHFACE_DOMAIN_DOTP 0

#undef WATCHFACE_TERRAIN
#define WATCHFACE_TERRAIN 0

#undef WATCHFACE_DOSY
#define WATCHFACE_DOSY 0

#undef WATCHFACE_SHADES_SZYBET
#define WATCHFACE_SHADES_SZYBET 0

#undef WATCHFACE_ANALOG_SHARP_SZYBET
#define WATCHFACE_ANALOG_SHARP_SZYBET 0

#undef WATCHFACE_PULSEPRO
#define WATCHFACE_PULSEPRO 1

#undef WATCHFACE_BINWATCH
#define WATCHFACE_BINWATCH 1

#undef WATCHFACE_ANALOG_PULSEPRO
#define WATCHFACE_ANALOG_PULSEPRO 1

#undef BOOK_MODULE
#define BOOK_MODULE 0

#undef IMAGE_MODULE
#define IMAGE_MODULE 0

#undef VAULT
#define VAULT 0

#undef PONG
#define PONG 0

#undef TETRIS
#define TETRIS 0

#undef JUMPER
#define JUMPER 0

#undef SNAKE
#define SNAKE 0

#undef PAINT
#define PAINT 0

#undef MAZE
#define MAZE 0

#undef CREDITS
#define CREDITS 0

#undef HEART_MONITOR
#define HEART_MONITOR 1

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
