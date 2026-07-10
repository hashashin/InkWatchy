#include "rtcDriftLab.h"

#if RTC_DRIFT_LAB_APP

#include "rtcMem.h"
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#define RTC_DRIFT_FONT_TITLE getFont("dogicapixel4")
#define RTC_DRIFT_FONT_BODY  getFont("UbuntuMono10")
#define RTC_DRIFT_FILE       "rtc_drift_lab"
#define RTC_DRIFT_PATH       "/conf/rtc_drift_lab"
#define RTC_DRIFT_MAGIC      UINT32_C(0x52444c31)

struct RtcDriftMeasurement
{
    uint32_t magic;
    uint32_t baselineNtp;
    int32_t baselineOffset;
    uint32_t latestNtp;
    int32_t latestOffset;
    uint16_t samples;
};

static RtcDriftMeasurement g_measurement = {};
static bool g_syncing = false;
static bool g_syncQueued = false;
static bool g_syncFailed = false;
static uint32_t g_syncPreviousNtp = 0;
static uint32_t g_queuedCapture = 0;
static volatile bool g_forceNextSample = false;
static volatile uint32_t g_latestCapturedNtp = 0;

static bool loadMeasurement(RtcDriftMeasurement& measurement)
{
    bufSize blob = fsGetBlob(RTC_DRIFT_FILE);
    if (blob.buf == nullptr) return false;

    bool valid = blob.size == sizeof(RtcDriftMeasurement);
    if (valid) {
        memcpy(&measurement, blob.buf, sizeof(measurement));
        valid = measurement.magic == RTC_DRIFT_MAGIC &&
                measurement.samples > 0 &&
                measurement.latestNtp >= measurement.baselineNtp;
    }
    free(blob.buf);
    return valid;
}

static bool saveMeasurement(const RtcDriftMeasurement& measurement)
{
    RtcDriftMeasurement stored = measurement;
    return fsSetBlob(RTC_DRIFT_FILE, (uint8_t*)&stored, sizeof(stored));
}

static void clearMeasurement()
{
    fsRemoveFile(RTC_DRIFT_PATH);
    g_measurement = {};
}

void rtcDriftLabRecordSync(int64_t rtcEpoch, int64_t ntpEpoch)
{
    if (rtcEpoch < 0 || ntpEpoch < 0 || ntpEpoch > UINT32_MAX) return;

    int64_t offset = rtcEpoch - ntpEpoch;
    if (offset < INT32_MIN || offset > INT32_MAX) return;

    RtcDriftMeasurement measurement = {};
    bool hasMeasurement = loadMeasurement(measurement);
    if (hasMeasurement &&
        !g_forceNextSample &&
        ntpEpoch > measurement.latestNtp &&
        ntpEpoch - measurement.latestNtp < (int64_t)RTC_DRIFT_LAB_SAMPLE_MINUTES * 60) {
        return;
    }

    if (!hasMeasurement || ntpEpoch <= measurement.baselineNtp) {
        measurement.magic = RTC_DRIFT_MAGIC;
        measurement.baselineNtp = (uint32_t)ntpEpoch;
        measurement.baselineOffset = (int32_t)offset;
        measurement.samples = 1;
    } else if (measurement.samples < UINT16_MAX) {
        measurement.samples++;
    }

    measurement.latestNtp = (uint32_t)ntpEpoch;
    measurement.latestOffset = (int32_t)offset;
    if (saveMeasurement(measurement)) {
        g_latestCapturedNtp = (uint32_t)ntpEpoch;
        g_forceNextSample = false;
    }
}

static String signedInteger(int32_t value)
{
    if (value >= 0) return "+" + String(value);
    return String(value);
}

static String signedDecimal(double value, uint8_t decimals)
{
    double threshold = 0.5;
    for (uint8_t i = 0; i < decimals; i++) threshold /= 10.0;
    if (fabs(value) < threshold) value = 0.0;

    String result;
    if (value >= 0.0) result = "+";
    result += String(value, (unsigned int)decimals);
    return result;
}

static String formatRtcTime()
{
    char buffer[9];
    snprintf(buffer, sizeof(buffer), "%02u:%02u:%02u",
             timeRTCUTC0.Hour, timeRTCUTC0.Minute, timeRTCUTC0.Second);
    return String(buffer);
}

static String formatBaseline(uint32_t epoch)
{
    tmElements_t time;
    time_t epochTime = (time_t)epoch;
    rM.SRTC.doBreakTime(epochTime, time);
    char buffer[15];
    snprintf(buffer, sizeof(buffer), "%02u.%02u %02u:%02u",
             time.Day, time.Month + 1, time.Hour, time.Minute);
    return String(buffer);
}

static String formatElapsed(uint32_t seconds)
{
    uint32_t days = seconds / 86400;
    uint8_t hours = (seconds % 86400) / 3600;
    uint8_t minutes = (seconds % 3600) / 60;

    char buffer[16];
    if (days > 0) {
        snprintf(buffer, sizeof(buffer), "%lud %02uh", (unsigned long)days, hours);
    } else {
        snprintf(buffer, sizeof(buffer), "%uh %02um", hours, minutes);
    }
    return String(buffer);
}

static const char* measurementStatus(bool hasMeasurement, uint32_t elapsed)
{
    if (g_syncQueued) return RTC_DRIFT_STATUS_WAITING;
    if (g_syncing) return RTC_DRIFT_STATUS_SYNCING;
    if (g_syncFailed) return RTC_DRIFT_STATUS_FAILED;
    if (!hasMeasurement) return RTC_DRIFT_STATUS_EMPTY;
    if (g_measurement.samples < 2) return RTC_DRIFT_STATUS_BASELINE;
    if (elapsed < (uint32_t)RTC_DRIFT_LAB_MIN_HOURS * 3600UL) return RTC_DRIFT_STATUS_MEASURING;
    return RTC_DRIFT_STATUS_READY;
}

static void printLine(int y, const char* label, const String& value)
{
    dis->setCursor(8, y);
    dis->print(label);
    dis->print(value);
}

static void renderRtcDriftLab()
{
    readRTC();
    bool hasMeasurement = loadMeasurement(g_measurement);
    uint32_t elapsed = 0;
    int32_t drift = 0;
    double rate = 0.0;
    double ppm = 0.0;

    if (hasMeasurement) {
        elapsed = g_measurement.latestNtp - g_measurement.baselineNtp;
        drift = g_measurement.latestOffset - g_measurement.baselineOffset;
        if (elapsed > 0) {
            rate = (double)drift * 86400.0 / (double)elapsed;
            ppm = (double)drift * 1000000.0 / (double)elapsed;
        }
    }

    dis->fillScreen(SCWhite);
    dis->setTextColor(SCBlack);
    dis->setTextSize(1);

    dis->setFont(RTC_DRIFT_FONT_TITLE);
    dis->setCursor(8, 14);
    dis->print(RTC_DRIFT_TITLE);
    dis->drawFastHLine(5, 22, 190, SCBlack);

    dis->setFont(RTC_DRIFT_FONT_BODY);
    printLine(40, RTC_DRIFT_LABEL_RTC, formatRtcTime());
    printLine(57, RTC_DRIFT_LABEL_STATUS, measurementStatus(hasMeasurement, elapsed));
    printLine(74, RTC_DRIFT_LABEL_BASE,
              hasMeasurement ? formatBaseline(g_measurement.baselineNtp) : "--.-- --:--");
    printLine(91, RTC_DRIFT_LABEL_ERROR,
              hasMeasurement ? signedInteger(g_measurement.latestOffset) + " s" : "--");
    printLine(108, RTC_DRIFT_LABEL_ELAPSED,
              hasMeasurement ? formatElapsed(elapsed) : "--");
    printLine(125, RTC_DRIFT_LABEL_DRIFT,
              hasMeasurement && g_measurement.samples > 1 ? signedInteger(drift) + " s" : "--");
    printLine(142, RTC_DRIFT_LABEL_RATE,
              elapsed > 0 ? signedDecimal(rate, 2) + " s/d" : "--");
    printLine(159, RTC_DRIFT_LABEL_PPM,
              elapsed > 0 ? signedDecimal(ppm, 1) + "  N=" + String(g_measurement.samples) : "--");

    dis->drawFastHLine(5, 169, 190, SCBlack);
    dis->setFont(RTC_DRIFT_FONT_TITLE);
    dis->setCursor(8, 194);
    dis->print(RTC_DRIFT_HINT);

    dUChange = true;
}

static void startRtcDriftSync()
{
    g_syncQueued = false;
    g_syncing = true;
    g_syncFailed = false;
    g_syncPreviousNtp = g_measurement.latestNtp;
    g_forceNextSample = true;
    renderRtcDriftLab();
    disUp(true);
    turnOnWifiNtpOnly();
}

void initRtcDriftLab()
{
    g_syncing = false;
    g_syncQueued = false;
    g_syncFailed = false;
    g_forceNextSample = false;
    renderRtcDriftLab();
    disUp(true);
}

void loopRtcDriftLab()
{
    buttonState button = useButton();
    resetSleepDelay(SLEEP_EVERY_MS);

    if ((button == Menu || button == LongMenu) && !g_syncing && !g_syncQueued) {
        g_syncFailed = false;
        if (isWifiTaskCheck()) {
            g_syncPreviousNtp = g_measurement.latestNtp;
            g_queuedCapture = g_latestCapturedNtp;
            g_forceNextSample = true;
            g_syncQueued = true;
            renderRtcDriftLab();
            disUp(true);
        } else {
            startRtcDriftSync();
        }
    } else if (button == Up && !isWifiTaskCheck() && !g_syncing && !g_syncQueued) {
        g_syncFailed = false;
        g_forceNextSample = false;
        clearMeasurement();
        renderRtcDriftLab();
    }

    if (g_syncQueued) {
        if (g_latestCapturedNtp != g_queuedCapture) {
            g_syncQueued = false;
            g_forceNextSample = false;
            renderRtcDriftLab();
            disUp(true);
        } else if (!isWifiTaskCheck()) {
            startRtcDriftSync();
        }
    }

    if (g_syncing && !isWifiTaskCheck()) {
        g_syncing = false;
        RtcDriftMeasurement syncedMeasurement = {};
        g_syncFailed = !loadMeasurement(syncedMeasurement) ||
                       syncedMeasurement.latestNtp == g_syncPreviousNtp;
        g_forceNextSample = false;
        renderRtcDriftLab();
    }

    disUp(false);
}

void exitRtcDriftLab()
{
}

#endif
