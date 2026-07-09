#include "moonSun.h"

#if MOON_SUN_APP

#include "rtcMem.h"
#include <MoonPhase.h>
#include <math.h>
#include <string.h>

#define DEG2RAD 0.01745329252f

static int g_lineTime = -1;

static float normalizeDegrees(float degrees)
{
    while (degrees < 0.0f) degrees += 360.0f;
    while (degrees >= 360.0f) degrees -= 360.0f;
    return degrees;
}

static int dayOfYear(int year, int month, int day)
{
    static const int monthOffsets[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    int doy = monthOffsets[month - 1] + day;
    if (month > 2) {
        bool leapYear = (year % 4 == 0) && (year % 100 != 0 || year % 400 == 0);
        if (leapYear) doy += 1;
    }
    return doy;
}

static float computeSolarEventMinutes(bool sunrise, float latitude, float longitude, int timezoneOffsetMinutes, int year, int month, int day)
{
    const float zenith = 90.833f;
    const float lngHour = longitude / 15.0f;
    const int n = dayOfYear(year, month, day);
    const float t = n + ((sunrise ? 6.0f : 18.0f) - lngHour) / 24.0f;
    const float meanAnomaly = 0.9856f * t - 3.289f;

    float trueLongitude = meanAnomaly +
                          1.916f * sinf(meanAnomaly * DEG2RAD) +
                          0.020f * sinf(2.0f * meanAnomaly * DEG2RAD) +
                          282.634f;
    trueLongitude = normalizeDegrees(trueLongitude);

    float rightAscension = atanf(0.91764f * tanf(trueLongitude * DEG2RAD)) / DEG2RAD;
    rightAscension = normalizeDegrees(rightAscension);
    const float lQuadrant = floorf(trueLongitude / 90.0f) * 90.0f;
    const float raQuadrant = floorf(rightAscension / 90.0f) * 90.0f;
    rightAscension += lQuadrant - raQuadrant;
    rightAscension /= 15.0f;

    const float sinDeclination = 0.39782f * sinf(trueLongitude * DEG2RAD);
    const float cosDeclination = cosf(asinf(sinDeclination));
    const float cosHourAngle = (cosf(zenith * DEG2RAD) - (sinDeclination * sinf(latitude * DEG2RAD))) /
                               (cosDeclination * cosf(latitude * DEG2RAD));

    if (cosHourAngle > 1.0f || cosHourAngle < -1.0f) return -1.0f;

    float hourAngle = sunrise ? 360.0f - (acosf(cosHourAngle) / DEG2RAD) : (acosf(cosHourAngle) / DEG2RAD);
    hourAngle /= 15.0f;

    float localMeanTime = hourAngle + rightAscension - 0.06571f * t - 6.622f;
    float universalTime = localMeanTime - lngHour;
    while (universalTime < 0.0f) universalTime += 24.0f;
    while (universalTime >= 24.0f) universalTime -= 24.0f;

    float localTime = universalTime + (timezoneOffsetMinutes / 60.0f);
    while (localTime < 0.0f) localTime += 24.0f;
    while (localTime >= 24.0f) localTime -= 24.0f;

    return localTime * 60.0f;
}

static bool hasLocation()
{
    return strlen(WEATHER_LATIT) > 0 && strlen(WEATHER_LONGTIT) > 0;
}

static String formatMinutes(float minutes)
{
    if (minutes < 0.0f) return "--:--";

    int rounded = (int)lroundf(minutes);
    rounded %= 1440;
    if (rounded < 0) rounded += 1440;

    char buf[6];
    snprintf(buf, sizeof(buf), "%02d:%02d", rounded / 60, rounded % 60);
    return String(buf);
}

static String formatDuration(float minutes)
{
    if (minutes < 0.0f) return "--:--";

    int rounded = (int)lroundf(minutes);
    char buf[8];
    snprintf(buf, sizeof(buf), "%dh%02d", rounded / 60, rounded % 60);
    return String(buf);
}

static void addLine(const char* label, const String& value)
{
    genpage_add((String(label) + value).c_str());
}

static const char* localizedMoonPhaseName(double phase)
{
    static const char* const phaseNames[] = {
        MOON_PHASE_NEW,
        MOON_PHASE_EVENING_CRESCENT,
        MOON_PHASE_FIRST_QUARTER,
        MOON_PHASE_WAXING_GIBBOUS,
        MOON_PHASE_FULL,
        MOON_PHASE_WANING_GIBBOUS,
        MOON_PHASE_LAST_QUARTER,
        MOON_PHASE_MORNING_CRESCENT
    };

    return phaseNames[(int)(phase * 8.0 + 0.5) % 8];
}


static String dateLine()
{
    return unixToDate(getUnixTime(timeRTCLocal)) + " " + getHourMinuteUnix(getUnixTime(timeRTCLocal));
}

static void renderMoonSun()
{
    init_general_page(10);
    general_page_set_title(MOON_SUN_TITLE);
    genpage_set_center();

    g_lineTime = genpage_add(dateLine().c_str());

    if (!hasLocation()) {
        genpage_add(MOON_SUN_NO_LOCATION);
    } else {
        float lat = String(WEATHER_LATIT).toFloat();
        float lon = String(WEATHER_LONGTIT).toFloat();
        int year = tmYearToCalendar(timeRTCLocal.Year);
        int month = (int)timeRTCLocal.Month + 1;
        int day = timeRTCLocal.Day;
        int tzMinutes = (int)(-timeZoneOffset / 60);

        float sunrise = computeSolarEventMinutes(true, lat, lon, tzMinutes, year, month, day);
        float sunset = computeSolarEventMinutes(false, lat, lon, tzMinutes, year, month, day);
        float daylight = (sunrise >= 0.0f && sunset >= 0.0f) ? (sunset - sunrise) : -1.0f;
        if (daylight < 0.0f && sunrise >= 0.0f && sunset >= 0.0f) daylight += 1440.0f;

        addLine(MOON_SUN_SUNRISE, formatMinutes(sunrise));
        addLine(MOON_SUN_SUNSET, formatMinutes(sunset));
        if (sunrise >= 0.0f && sunset >= 0.0f) {
            addLine(MOON_SUN_SOLAR_NOON, formatMinutes(sunrise + daylight / 2.0f));
        } else {
            addLine(MOON_SUN_SOLAR_NOON, "--:--");
        }
        addLine(MOON_SUN_DAYLIGHT, formatDuration(daylight));
    }

    MoonPhase mp;
    mp.calculate(getUnixTime(timeRTCLocal));
    addLine(MOON_SUN_MOON, String(localizedMoonPhaseName(mp.phase)));
    addLine(MOON_SUN_ILLUM, String((int)lroundf(mp.fraction * 100.0f)) + "%");

    if (hasLocation()) {
        addLine(MOON_SUN_LOCATION, String(WEATHER_LATIT) + "," + String(WEATHER_LONGTIT));
    }

    general_page_set_main();
}

void initMoonSun()
{
    renderMoonSun();
}

void loopMoonSun()
{
    resetSleepDelay(SLEEP_EVERY_MS);
    String now = dateLine();
    if (g_lineTime >= 0) {
        genpage_change(now.c_str(), g_lineTime);
    }
    general_page_set_main();
    slint_loop();
}

void exitMoonSun()
{
    slintExit();
}

#endif
