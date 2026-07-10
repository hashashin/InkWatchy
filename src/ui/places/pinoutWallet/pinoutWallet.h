#pragma once

#include "defines.h"

#ifndef PINOUT_WALLET_APP
#define PINOUT_WALLET_APP 0
#endif

#if PINOUT_WALLET_APP
void initPinoutWallet();
void loopPinoutWallet();
void exitPinoutWallet();
#endif
