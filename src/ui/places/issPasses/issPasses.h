#pragma once

#include "defines.h"

#ifndef ISS_PASSES_APP
#define ISS_PASSES_APP 0
#endif

#if ISS_PASSES_APP

#ifndef ISS_PASSES_ENDPOINT
#define ISS_PASSES_ENDPOINT "https://iss-api.polluxlabs.io/iss-pass"
#endif

#ifndef ISS_PASSES_COUNT
#define ISS_PASSES_COUNT 5
#endif

#ifndef ISS_PASSES_DAYS_AHEAD
#define ISS_PASSES_DAYS_AHEAD 14
#endif

#ifndef ISS_PASSES_MIN_ELEVATION
#define ISS_PASSES_MIN_ELEVATION 10
#endif

#ifndef ISS_PASSES_VISIBLE_ONLY
#define ISS_PASSES_VISIBLE_ONLY 1
#endif

#ifndef ISS_PASSES_CACHE_FILE
#define ISS_PASSES_CACHE_FILE "iss_passes_v1"
#endif

void initIssPasses();
void loopIssPasses();
void exitIssPasses();

#endif
