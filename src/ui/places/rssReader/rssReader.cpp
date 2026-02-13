#include "rssReader.h"

#if RSS_READER

#include <WiFiClientSecure.h>
#include <qrcode.h>

#ifndef RSS_READER_FONT
#define RSS_READER_FONT getFont("UbuntuMono10")
#endif

static const char* kEndpoint  = RSS_READER_ENDPOINT;
static const char* kCachePath = RSS_READER_CACHE_PATH;

static constexpr int kHeaderH = 18;
static constexpr int kPadX    = 2;
static constexpr int kPadY    = 3;
static constexpr int kGapY    = 2;

// -----------------------------------------------------------------------------
// Font metrics (computed)
// -----------------------------------------------------------------------------
static int g_lineH = 13;
static int g_itemH = 26;

// -----------------------------------------------------------------------------
// State
// -----------------------------------------------------------------------------
static bool g_viewerMode = false;
static int  g_selected = 0;
static int  g_scroll = 0;

enum class FetchState : uint8_t {
    Unknown = 0,
    Ok,
    Offline,
    WifiOff,
    Fail,
};

static bool g_fetching = false;
static bool g_dirty = false;
static FetchState g_state = FetchState::Unknown;

static std::vector<std::string> g_titles;
static std::vector<std::string> g_urls;

static std::string g_src; // from JSON (src)

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------
static void trim(std::string& s)
{
    while (!s.empty() && (s.back()=='\n' || s.back()=='\r' || s.back()==' ' || s.back()=='\t')) s.pop_back();
    size_t i = 0;
    while (i < s.size() && (s[i]==' ' || s[i]=='\t')) i++;
    if (i) s.erase(0, i);
}

static void ensureBounds()
{
    if (g_titles.empty()) { g_selected = 0; g_scroll = 0; return; }
    if (g_selected < 0) g_selected = 0;
    if (g_selected >= (int)g_titles.size()) g_selected = (int)g_titles.size() - 1;
    if (g_scroll < 0) g_scroll = 0;
    if (g_scroll > g_selected) g_scroll = g_selected;
}

static void computeFontMetrics()
{
    setFont(RSS_READER_FONT);
    setTextSize(1);

    int16_t x1 = 0, y1 = 0;
    uint16_t w = 0, h = 0;

    // Use descenders to get a stable height
    String probe = "Ag";
    getTextBounds(probe, &x1, &y1, &w, &h);

    int lh = (int)h;
    if (lh < 10) lh = 10;
    if (lh > 24) lh = 24;

    g_lineH = lh + 1;
    g_itemH = (g_lineH * 2) + (kPadY * 2) + kGapY;

    // Make sure we always get at least 2 items visible on 200x200
    if (g_itemH < 24) g_itemH = 24;
    if (g_itemH > 40) g_itemH = 40;
}

static void wrap2(const std::string& in, std::string& l1, std::string& l2, int maxChars)
{
    std::string s = in;
    trim(s);

    const int max1 = maxChars;
    const int max2 = maxChars;

    if ((int)s.size() <= max1) {
        l1 = s;
        l2.clear();
        return;
    }

    size_t cut = s.rfind(' ', (size_t)max1);
    if (cut == std::string::npos || cut < 10) cut = (size_t)max1;

    l1 = s.substr(0, cut);
    trim(l1);

    std::string rest = s.substr(cut);
    trim(rest);

    if ((int)rest.size() <= max2) {
        l2 = rest;
    } else {
        l2 = rest.substr(0, (size_t)max2 - 3);
        l2 += "...";
    }
}

// -----------------------------------------------------------------------------
// Cache I/O
// -----------------------------------------------------------------------------
static void saveToCache(const String& body)
{
    if (!LittleFS.exists("/rss")) {
        LittleFS.mkdir("/rss");
    }
    File f = LittleFS.open(kCachePath, "w");
    if (!f) return;
    f.print(body);
    f.close();
}

static bool loadFromCache()
{
    if (!LittleFS.exists(kCachePath)) return false;
    File f = LittleFS.open(kCachePath, "r");
    if (!f) return false;

    DynamicJsonDocument doc(8192);
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) return false;

    g_titles.clear();
    g_urls.clear();
    g_src.clear();

    const char* src = doc["src"] | "";
    if (src && src[0]) {
        g_src = src;
        trim(g_src);
    }

    JsonArray items = doc["items"].as<JsonArray>();
    if (items.isNull()) return false;

    for (JsonVariant v : items)
    {
        const char* t = v["t"] | "";
        const char* u = v["u"] | "";
        if (!t || !u) continue;

        std::string ts(t), us(u);
        trim(ts); trim(us);
        if (ts.empty() || us.empty()) continue;

        g_titles.push_back(ts);
        g_urls.push_back(us);

        if ((int)g_titles.size() >= 24) break;
    }

    ensureBounds();
    return !g_titles.empty();
}

// -----------------------------------------------------------------------------
// QR (same pattern as qrApp)
// -----------------------------------------------------------------------------
static void drawQrHandle(esp_qrcode_handle_t qrcode)
{
    const int border = 2;

    const int size = esp_qrcode_get_size(qrcode);
    const int modules = size + border * 2;

    const int w = dis->width();
    const int h = dis->height();

    int scaleX = w / modules;
    int scaleY = h / modules;
    int scale = (scaleX < scaleY) ? scaleX : scaleY;
    if (scale < 1) scale = 1;

    const int px = modules * scale;
    const int x0 = (w - px) / 2;
    const int y0 = (h - px) / 2;

    dis->fillRect(x0, y0, px, px, SCWhite);

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            if (esp_qrcode_get_module(qrcode, x, y)) {
                dis->fillRect(
                    x0 + (x + border) * scale,
                    y0 + (y + border) * scale,
                    scale, scale,
                    SCBlack
                );
            }
        }
    }
}

static void onQrReady(esp_qrcode_handle_t qrcode)
{
    drawQrHandle(qrcode);
}

static void renderViewer()
{
    dis->fillScreen(SCWhite);

    if (g_selected < 0) g_selected = 0;
    if (g_selected >= (int)g_urls.size()) g_selected = (int)g_urls.size() - 1;

    const char* txt = g_urls[g_selected].c_str();

    esp_qrcode_config_t cfg = {
        .display_func = onQrReady,
        .max_qrcode_version = 10,
        .qrcode_ecc_level = ESP_QRCODE_ECC_MED,
    };

    if (esp_qrcode_generate(&cfg, txt) != ESP_OK) {
        writeTextCenterReplaceBack("QR error", dis->height() / 2);
    }

    disUp(true);
}

// -----------------------------------------------------------------------------
// UI
// -----------------------------------------------------------------------------
static const char* stateToStr()
{
    if (g_fetching) return "...";
    switch (g_state) {
        case FetchState::Ok:      return "OK";
        case FetchState::Offline: return "OFFLINE";
        case FetchState::WifiOff: return "WIFI OFF";
        case FetchState::Fail:    return "FAIL";
        default:                  return "";
    }
}

static void renderList()
{
    dis->fillScreen(SCWhite);

    setFont(RSS_READER_FONT);
    setTextSize(1);

    dis->setCursor(kPadX, 12);
    if (!g_src.empty()) {
        dis->print(g_src.c_str());
    } else {
        dis->print("RSS");
    }

    String st = stateToStr();
    uint16_t tw=0, th=0;
    getTextBounds(st, NULL, NULL, &tw, &th);
    dis->setCursor(dis->width() - (int)tw - kPadX, 12);
    dis->print(st);

    dis->fillRect(0, 16, dis->width(), 1, SCBlack);

    const int top = kHeaderH;
    const int bottom = dis->height() - 2;
    int visible = (bottom - top) / g_itemH;
    if (visible < 1) visible = 1;

    // Approx chars per line based on display width and measured glyph width
    int16_t x1 = 0, y1 = 0;
    uint16_t w = 0, h = 0;
    String probe = "0000000000";
    getTextBounds(probe, &x1, &y1, &w, &h);
    float avgChar = (w > 0) ? (float)w / 10.0f : 6.0f;
    int maxChars = (int)((dis->width() - (kPadX * 2)) / avgChar);
    if (maxChars < 18) maxChars = 18;
    if (maxChars > 40) maxChars = 40;

    if (g_selected < g_scroll) g_scroll = g_selected;
    if (g_selected >= g_scroll + visible) g_scroll = g_selected - visible + 1;
    if (g_scroll < 0) g_scroll = 0;

    for (int row = 0; row < visible; row++)
    {
        int idx = g_scroll + row;
        if (idx >= (int)g_titles.size()) break;

        const bool sel = (idx == g_selected);
        const int y = top + row * g_itemH;

        if (sel) {
            dis->fillRect(0, y, dis->width(), g_itemH, SCBlack);
            dis->setTextColor(SCWhite);
        } else {
            dis->setTextColor(SCBlack);
        }

        std::string l1, l2;
        wrap2(g_titles[idx], l1, l2, maxChars);

        // Baselines derived from computed line height; avoids overlap for any GFX font
        const int y1b = y + kPadY + (g_lineH - 1);
        const int y2b = y1b + g_lineH + kGapY;

        dis->setCursor(kPadX, y1b);
        dis->print(l1.c_str());

        if (!l2.empty()) {
            dis->setCursor(kPadX, y2b);
            dis->print(l2.c_str());
        }

        if (sel) dis->setTextColor(SCBlack);
    }

    dUChange = true;
}

// -----------------------------------------------------------------------------
// Wifi task fetch
// -----------------------------------------------------------------------------
static void wifiTaskFetchRss()
{
    if (WiFi.status() != WL_CONNECTED) {
        g_fetching = false;
        g_state = (g_titles.empty() ? FetchState::Fail : FetchState::Offline);
        g_dirty = true;
        return;
    }

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setTimeout(8000);

    if (!http.begin(client, kEndpoint)) {
        g_fetching = false;
        g_state = (g_titles.empty() ? FetchState::Fail : FetchState::Offline);
        g_dirty = true;
        return;
    }

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        http.end();
        g_fetching = false;
        g_state = (g_titles.empty() ? FetchState::Fail : FetchState::Offline);
        g_dirty = true;
        return;
    }

    String body = http.getString();
    http.end();

    if (body.length() < 5) {
        g_fetching = false;
        g_state = (g_titles.empty() ? FetchState::Fail : FetchState::Offline);
        g_dirty = true;
        return;
    }

    {
        DynamicJsonDocument doc(8192);
        if (deserializeJson(doc, body)) {
            g_fetching = false;
            g_state = (g_titles.empty() ? FetchState::Fail : FetchState::Offline);
            g_dirty = true;
            return;
        }
        if (doc["items"].isNull()) {
            g_fetching = false;
            g_state = (g_titles.empty() ? FetchState::Fail : FetchState::Offline);
            g_dirty = true;
            return;
        }
    }

    saveToCache(body);
    loadFromCache();
    g_state = (!g_titles.empty() ? FetchState::Ok : FetchState::Fail);

    g_fetching = false;
    g_dirty = true;
}

static void startManualRefresh()
{
    if (isWifiTaskCheck()) return;

    g_fetching = true;
    g_state = FetchState::Unknown;
    g_dirty = true;

    createWifiTask(WIFI_CONNECTION_TRIES, wifiTaskFetchRss, WIFI_PRIORITY_REGULAR);
}

// -----------------------------------------------------------------------------
// Lifecycle
// -----------------------------------------------------------------------------
void initRssReader()
{
    g_viewerMode = false;
    g_fetching = false;
    g_state = FetchState::Unknown;
    g_dirty = true;

    computeFontMetrics();

    if (loadFromCache()) {
        g_state = FetchState::Offline;
    } else {
        g_state = FetchState::Offline;
    }

    renderList();
    g_dirty = false;
}

void loopRssReader()
{
    if (g_fetching && !isWifiTaskCheck() && WiFi.status() != WL_CONNECTED) {
        g_fetching = false;
        g_state = (g_titles.empty() ? FetchState::Fail : FetchState::Offline);
        g_dirty = true;
    }

    if (g_dirty) {
        renderList();
        g_dirty = false;
    }
    resetSleepDelay(SLEEP_EVERY_MS);
    buttonState btn = useButton();

    if (!g_viewerMode)
    {
        if (btn == Up) {
            if (!g_titles.empty()) {
                g_selected--;
                if (g_selected < 0) g_selected = (int)g_titles.size() - 1;
                g_dirty = true;
                delayTask(120);
            }
        }
        else if (btn == Down) {
            if (!g_titles.empty()) {
                g_selected++;
                if (g_selected >= (int)g_titles.size()) g_selected = 0;
                g_dirty = true;
                delayTask(120);
            }
        }
        else if (btn == Menu) {
            if (!g_titles.empty()) {
                g_viewerMode = true;
                renderViewer();
            }
        }
        else if (btn == LongMenu) {
            startManualRefresh();
        }
        else if (btn == Back) {
            exitRssReader();
            return;
        }
    }
    else
    {
        if (btn == Back || btn == Menu) {
            g_viewerMode = false;
            g_dirty = true;
        }
        else if (btn == LongMenu) {
            g_viewerMode = false;
            g_dirty = true;
            startManualRefresh();
        }
    }

    disUp();
}

void exitRssReader()
{
    switchBack();
}

#endif
