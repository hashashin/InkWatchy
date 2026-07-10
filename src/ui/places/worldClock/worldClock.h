#pragma once

#include "defines.h"

#ifndef WORLD_CLOCK_APP
#define WORLD_CLOCK_APP 0
#endif

#if WORLD_CLOCK_APP
void initWorldClock();
void loopWorldClock();
void exitWorldClock();
#endif
