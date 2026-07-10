#pragma once

#include "defines.h"

#ifndef RTC_DRIFT_LAB_APP
#define RTC_DRIFT_LAB_APP 0
#endif

#ifndef RTC_DRIFT_LAB_MIN_HOURS
#define RTC_DRIFT_LAB_MIN_HOURS 24
#endif

#ifndef RTC_DRIFT_LAB_SAMPLE_MINUTES
#define RTC_DRIFT_LAB_SAMPLE_MINUTES 60
#endif

#if RTC_DRIFT_LAB_APP
void initRtcDriftLab();
void loopRtcDriftLab();
void exitRtcDriftLab();
void rtcDriftLabRecordSync(int64_t rtcEpoch, int64_t ntpEpoch);
#endif
