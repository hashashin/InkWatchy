#include "issPasses.h"

#if ISS_PASSES_APP

#include "rtcMem.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#define ISS_PASSES_CACHE_MAGIC 0x49535350UL
#define ISS_PASSES_CACHE_VERSION 1
#define ISS_PASSES_FONT_TITLE getFont("dogicapixel4")
#define ISS_PASSES_FONT_BODY getFont("UbuntuMono10")

static_assert(ISS_PASSES_COUNT >= 1 && ISS_PASSES_COUNT <= 20,
              "ISS_PASSES_COUNT must be between 1 and 20");

struct IssPassRecord
{
    uint64_t riseUtc;
    uint64_t setUtc;
    uint16_t durationSec;
    uint8_t maxElevation;
    char riseCompass[4];
    char setCompass[4];
};

struct IssPassCache
{
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint64_t generatedUtc;
    uint8_t count;
    IssPassRecord passes[ISS_PASSES_COUNT];
};

enum class IssState : uint8_t
{
    Empty,
    Cached,
    Syncing,
    Online,
    Failed,
    Busy,
    NoLocation,
};

static IssPassCache g_cache = {};
static uint8_t g_upcoming[ISS_PASSES_COUNT] = {};
static uint8_t g_upcomingCount = 0;
static uint8_t g_selected = 0;
static IssState g_state = IssState::Empty;
static bool g_fetching = false;
static bool g_dirty = true;

static bool hasLocation()
{
    return strlen(WEATHER_LATIT) > 0 && strlen(WEATHER_LONGTIT) > 0;
}

static int64_t daysFromCivil(int year, unsigned month, unsigned day)
{
    year -= month <= 2;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yearOfEra = static_cast<unsigned>(year - era * 400);
    const unsigned adjustedMonth = month > 2 ? month - 3 : month + 9;
    const unsigned dayOfYear = (153 * adjustedMonth + 2) / 5 + day - 1;
    const unsigned dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
    return era * 146097LL + static_cast<int64_t>(dayOfEra) - 719468LL;
}

static bool parseIsoUtc(const char *value, uint64_t &result)
{
    if (value == nullptr)
    {
        return false;
    }

    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    if (sscanf(value, "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &minute, &second) != 6 ||
        year < 1970 || month < 1 || month > 12 || day < 1 || day > 31 ||
        hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 60)
    {
        return false;
    }

    const int64_t days = daysFromCivil(year, static_cast<unsigned>(month), static_cast<unsigned>(day));
    if (days < 0)
    {
        return false;
    }
    result = static_cast<uint64_t>(days * 86400LL + hour * 3600 + minute * 60 + second);
    return true;
}

static void copyCompass(char destination[4], const char *source)
{
    memset(destination, 0, 4);
    if (source == nullptr || source[0] == '\0')
    {
        destination[0] = '?';
        return;
    }
    strncpy(destination, source, 3);
}

static bool populateCache(JsonDocument &doc, IssPassCache &cache)
{
    JsonArray passes = doc["passes"].as<JsonArray>();
    if (passes.isNull())
    {
        return false;
    }

    cache = {};
    cache.magic = ISS_PASSES_CACHE_MAGIC;
    cache.version = ISS_PASSES_CACHE_VERSION;
    cache.size = sizeof(IssPassCache);
    parseIsoUtc(doc["generated_at"] | "", cache.generatedUtc);

    for (JsonVariant pass : passes)
    {
        if (cache.count >= ISS_PASSES_COUNT)
        {
            break;
        }

        IssPassRecord &record = cache.passes[cache.count];
        if (!parseIsoUtc(pass["rise"]["time"] | "", record.riseUtc) ||
            !parseIsoUtc(pass["set"]["time"] | "", record.setUtc))
        {
            continue;
        }

        int elevation = lroundf(pass["culmination"]["elevation_deg"] | 0.0f);
        record.maxElevation = static_cast<uint8_t>(constrain(elevation, 0, 90));
        int duration = pass["visible_duration_sec"] | 0;
        if (duration <= 0)
        {
            duration = pass["duration_sec"] | 0;
        }
        record.durationSec = static_cast<uint16_t>(constrain(duration, 0, 65535));
        copyCompass(record.riseCompass, pass["rise"]["compass"] | "?");
        copyCompass(record.setCompass, pass["set"]["compass"] | "?");
        cache.count++;
    }

    if (cache.generatedUtc == 0)
    {
        readRTC();
        cache.generatedUtc = getUnixTime(timeRTCUTC0);
    }
    return true;
}

static bool loadCache()
{
    bufSize blob = fsGetBlob(ISS_PASSES_CACHE_FILE);
    if (blob.buf == nullptr)
    {
        return false;
    }

    bool valid = false;
    if (blob.size == sizeof(IssPassCache))
    {
        IssPassCache stored;
        memcpy(&stored, blob.buf, sizeof(stored));
        if (stored.magic == ISS_PASSES_CACHE_MAGIC &&
            stored.version == ISS_PASSES_CACHE_VERSION &&
            stored.size == sizeof(IssPassCache) &&
            stored.count <= ISS_PASSES_COUNT)
        {
            g_cache = stored;
            valid = true;
        }
    }
    free(blob.buf);
    return valid;
}

static bool saveCache(const IssPassCache &cache)
{
    return fsSetBlob(ISS_PASSES_CACHE_FILE,
                     reinterpret_cast<uint8_t *>(const_cast<IssPassCache *>(&cache)),
                     sizeof(cache));
}

static void rebuildUpcoming()
{
    readRTC();
    const uint64_t nowUtc = getUnixTime(timeRTCUTC0);
    g_upcomingCount = 0;
    for (uint8_t i = 0; i < g_cache.count; i++)
    {
        if (g_cache.passes[i].setUtc + 60 >= nowUtc)
        {
            g_upcoming[g_upcomingCount++] = i;
        }
    }
    if (g_upcomingCount == 0)
    {
        g_selected = 0;
    }
    else if (g_selected >= g_upcomingCount)
    {
        g_selected = g_upcomingCount - 1;
    }
}

static const char *stateText()
{
    switch (g_state)
    {
    case IssState::Cached:
        return ISS_PASSES_CACHED;
    case IssState::Syncing:
        return ISS_PASSES_SYNCING;
    case IssState::Online:
        return ISS_PASSES_ONLINE;
    case IssState::Failed:
        return ISS_PASSES_FAILED;
    case IssState::Busy:
        return ISS_PASSES_BUSY;
    default:
        return "";
    }
}

static void drawCentered(const String &text, int16_t baseline, const GFXfont *font)
{
    dis->setFont(font);
    int16_t x1 = 0;
    int16_t y1 = 0;
    uint16_t width = 0;
    uint16_t height = 0;
    dis->getTextBounds(text, 0, baseline, &x1, &y1, &width, &height);
    dis->setCursor((200 - static_cast<int16_t>(width)) / 2, baseline);
    dis->print(text);
}

static String localDateTime(uint64_t utc)
{
    time_t local = static_cast<time_t>(utc) - timeZoneOffset;
    tmElements_t time;
    rM.SRTC.doBreakTime(local, time);
    char buffer[15];
    snprintf(buffer, sizeof(buffer), "%02u.%02u %02u:%02u",
             time.Day, time.Month + 1, time.Hour, time.Minute);
    return String(buffer);
}

static String durationText(uint16_t durationSec)
{
    char buffer[12];
    snprintf(buffer, sizeof(buffer), "%um %02us", durationSec / 60, durationSec % 60);
    return String(buffer);
}

static String untilText(uint64_t riseUtc)
{
    const int64_t nowUtc = getUnixTime(timeRTCUTC0);
    const int64_t seconds = static_cast<int64_t>(riseUtc) - nowUtc;
    if (seconds <= 60)
    {
        return ISS_PASSES_NOW;
    }

    const int64_t minutes = seconds / 60;
    if (minutes < 60)
    {
        return String(ISS_PASSES_IN) + String(minutes) + "m";
    }
    if (minutes < 1440)
    {
        return String(ISS_PASSES_IN) + String(minutes / 60) + "h " + String(minutes % 60) + "m";
    }
    return String(ISS_PASSES_IN) + String(minutes / 1440) + "d " + String((minutes % 1440) / 60) + "h";
}

static void renderIssPasses()
{
    rebuildUpcoming();
    dis->fillScreen(SCWhite);
    dis->setTextColor(SCBlack);
    dis->setTextWrap(false);

    dis->setFont(ISS_PASSES_FONT_TITLE);
    dis->setCursor(7, 14);
    dis->print(ISS_PASSES_TITLE);

    String status = stateText();
    int16_t x1 = 0;
    int16_t y1 = 0;
    uint16_t width = 0;
    uint16_t height = 0;
    dis->getTextBounds(status, 0, 14, &x1, &y1, &width, &height);
    dis->setCursor(194 - width, 14);
    dis->print(status);
    dis->drawFastHLine(5, 21, 190, SCBlack);

    if (!hasLocation())
    {
        drawCentered(ISS_PASSES_NO_LOCATION, 86, ISS_PASSES_FONT_BODY);
    }
    else if (g_upcomingCount == 0)
    {
        drawCentered(ISS_PASSES_EMPTY, 86, ISS_PASSES_FONT_BODY);
        drawCentered(ISS_PASSES_REFRESH, 112, ISS_PASSES_FONT_BODY);
    }
    else
    {
        const IssPassRecord &pass = g_cache.passes[g_upcoming[g_selected]];
        drawCentered(String(ISS_PASSES_PASS) + String(g_selected + 1) + "/" + String(g_upcomingCount),
                     42, ISS_PASSES_FONT_TITLE);
        drawCentered(localDateTime(pass.riseUtc), 70, ISS_PASSES_FONT_BODY);
        drawCentered(String(pass.riseCompass) + "  >  " + String(pass.setCompass),
                     99, ISS_PASSES_FONT_BODY);
        drawCentered(String(ISS_PASSES_MAX) + String(pass.maxElevation) + "deg",
                     126, ISS_PASSES_FONT_BODY);
        drawCentered(String(ISS_PASSES_DURATION) + durationText(pass.durationSec),
                     151, ISS_PASSES_FONT_BODY);
        drawCentered(untilText(pass.riseUtc), 176, ISS_PASSES_FONT_BODY);
    }

    dis->drawFastHLine(5, 184, 190, SCBlack);
    drawCentered(ISS_PASSES_HINT, 197, ISS_PASSES_FONT_TITLE);
    dUChange = true;
}

static String requestUrl()
{
    String url = ISS_PASSES_ENDPOINT;
    url += "?lat=" + String(WEATHER_LATIT);
    url += "&lon=" + String(WEATHER_LONGTIT);
    url += "&n=" + String(ISS_PASSES_COUNT);
    url += "&days_ahead=" + String(ISS_PASSES_DAYS_AHEAD);
    url += "&min_elevation=" + String(ISS_PASSES_MIN_ELEVATION);
    url += ISS_PASSES_VISIBLE_ONLY ? "&visible_only=true" : "&visible_only=false";
    return url;
}

static void finishFetch(IssState state)
{
    g_fetching = false;
    g_state = state;
    g_dirty = true;
}

static void wifiTaskFetchIssPasses()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        finishFetch(IssState::Failed);
        return;
    }

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setTimeout(10000);
    if (!http.begin(client, requestUrl()))
    {
        finishFetch(IssState::Failed);
        return;
    }

    const int code = http.GET();
    if (code != HTTP_CODE_OK)
    {
        debugLog("ISS Passes HTTP error: " + String(code));
        http.end();
        finishFetch(IssState::Failed);
        return;
    }

    DynamicJsonDocument doc(8192);
    const DeserializationError error = deserializeJson(doc, http.getStream());
    http.end();
    if (error)
    {
        debugLog("ISS Passes JSON error: " + String(error.c_str()));
        finishFetch(IssState::Failed);
        return;
    }

    IssPassCache downloaded;
    if (!populateCache(doc, downloaded) || !saveCache(downloaded))
    {
        finishFetch(IssState::Failed);
        return;
    }

    g_cache = downloaded;
    g_selected = 0;
    finishFetch(IssState::Online);
}

static void startRefresh()
{
    if (!hasLocation())
    {
        g_state = IssState::NoLocation;
        g_dirty = true;
        return;
    }
    if (isWifiTaskCheck())
    {
        g_state = IssState::Busy;
        g_dirty = true;
        return;
    }

    g_fetching = true;
    g_state = IssState::Syncing;
    g_dirty = true;
    createWifiTask(WIFI_CONNECTION_TRIES, wifiTaskFetchIssPasses, WIFI_PRIORITY_REGULAR);
}

void initIssPasses()
{
    g_cache = {};
    g_selected = 0;
    g_fetching = false;
    g_dirty = true;

    if (!hasLocation())
    {
        g_state = IssState::NoLocation;
    }
    else if (loadCache())
    {
        g_state = IssState::Cached;
    }
    else
    {
        g_state = IssState::Empty;
        startRefresh();
    }

    renderIssPasses();
    g_dirty = false;
    disUp(true);
}

void loopIssPasses()
{
    if (g_fetching && !isWifiTaskCheck())
    {
        finishFetch(IssState::Failed);
    }

    const buttonState button = useButton();
    if (button == Menu)
    {
        startRefresh();
    }
    else if (button == Up && g_upcomingCount > 1)
    {
        g_selected = (g_selected + g_upcomingCount - 1) % g_upcomingCount;
        g_dirty = true;
    }
    else if (button == Down && g_upcomingCount > 1)
    {
        g_selected = (g_selected + 1) % g_upcomingCount;
        g_dirty = true;
    }

    if (g_dirty)
    {
        g_dirty = false;
        renderIssPasses();
    }

    resetSleepDelay(SLEEP_EVERY_MS);
    disUp(false);
}

void exitIssPasses()
{
}

#endif
