#include "qrApp.h"

#if QR_APP

#include <qrcode.h>
#include <esp_err.h>

#include <vector>
#include <string>

static bool g_loaded = false;
static const char* kQrListPath = "/qrapp/qrlist.txt";

// Menu state
static int g_selected = 0;
static bool g_viewerMode = false;

// Parsed entries
static std::vector<std::string> g_titles;
static std::vector<std::string> g_payloads;

static void trimLine(std::string& s)
{
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ' || s.back() == '\t')) {
        s.pop_back();
    }
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) i++;
    if (i > 0) s.erase(0, i);
}

static void pushEntryFromLine(const std::string& lineIn)
{
    std::string line = lineIn;
    trimLine(line);
    if (line.empty()) return;
    if (line[0] == '#') return;

    // Format: Title|payload
    // If no '|', use whole line as both title and payload.
    auto split = line.find('|');

    std::string title;
    std::string payload;

    if (split == std::string::npos) {
        title = line;
        payload = line;
    } else {
        title = line.substr(0, split);
        payload = line.substr(split + 1);
        trimLine(title);
        trimLine(payload);

        if (title.empty()) title = payload;
        if (payload.empty()) payload = title;
    }

    g_titles.push_back(title);
    g_payloads.push_back(payload);
}

static void loadQrListOnce()
{
    if (g_loaded) return;
    g_loaded = true;

    g_titles.clear();
    g_payloads.clear();

    if (!LittleFS.exists(kQrListPath)) {
        g_titles.emplace_back("Missing qrlist.txt");
        g_payloads.emplace_back("Missing qrlist.txt");
        g_titles.emplace_back("Add: Title|payload");
        g_payloads.emplace_back("Add: Title|payload");
        return;
    }

    File f = LittleFS.open(kQrListPath, "r");
    if (!f) {
        g_titles.emplace_back("Open qrlist failed");
        g_payloads.emplace_back("Open qrlist failed");
        return;
    }

    std::string line;
    line.reserve(256);

    while (f.available()) {
        int c = f.read();
        if (c < 0) break;

        if (c == '\n') {
            pushEntryFromLine(line);
            line.clear();
        } else {
            if (line.size() < 2048) {
                line.push_back((char)c);
            }
        }
    }

    pushEntryFromLine(line);

    f.close();

    if (g_titles.empty()) {
        g_titles.emplace_back("qrlist.txt empty");
        g_payloads.emplace_back("qrlist.txt empty");
    }

    if (g_selected < 0 || g_selected >= (int)g_titles.size()) g_selected = 0;
}

// ---- QR drawing ----

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

// ---- UI ----

static void renderMenu()
{
    dis->fillScreen(SCWhite);

    loadQrListOnce();

    setFont(font);
    setTextSize(1);

    const int lineH = 14;

    // Header
    const char* kHeader = "QR LIST";
    const int headerY = 12;        // baseline aproximada
    const int headerPadBottom = 6; // espacio debajo del header

    int headerX = 4;
    // Centrado usando getTextBounds si existe (Adafruit_GFX)
    // Si tu display class no lo soporta, usa fallback simple.
    {
        int16_t x1, y1;
        uint16_t tw, th;
        // Many InkWatchy displays inherit Adafruit_GFX so this exists.
        dis->getTextBounds(kHeader, 0, 0, &x1, &y1, &tw, &th);
        headerX = (dis->width() - (int)tw) / 2;
        if (headerX < 0) headerX = 0;
    }

    dis->setTextColor(SCBlack);
    dis->setCursor(headerX, headerY);
    dis->print(kHeader);

    const int listTop = headerY + headerPadBottom + 8;
    int y = listTop;

    for (int i = 0; i < (int)g_titles.size(); i++) {
        if (y + lineH > dis->height() - 12) break; // deja espacio para footer

        if (i == g_selected) {
            dis->fillRect(0, y - (lineH - 2), dis->width(), lineH, SCBlack);
            dis->setTextColor(SCWhite);
        } else {
            dis->setTextColor(SCBlack);
        }

        dis->setCursor(6, y);
        dis->print(g_titles[i].c_str());

        y += lineH;
    }

    // Footer count
    dis->setTextColor(SCBlack);
    dis->setCursor(4, dis->height() - 4);
    dis->print(String(g_selected + 1) + "/" + String((int)g_titles.size()));

    disUp(true);
}

static void renderViewer()
{
    dis->fillScreen(SCWhite);

    loadQrListOnce();

    if (g_selected < 0) g_selected = 0;
    if (g_selected >= (int)g_payloads.size()) g_selected = (int)g_payloads.size() - 1;

    const char* txt = g_payloads[g_selected].c_str();

    esp_qrcode_config_t cfg = {
        .display_func = onQrReady,
        .max_qrcode_version = 10,
        .qrcode_ecc_level = ESP_QRCODE_ECC_MED,
    };

    esp_err_t err = esp_qrcode_generate(&cfg, txt);
    if (err != ESP_OK) {
        setFont(font);
        setTextSize(1);
        writeTextCenterReplaceBack("QR error", dis->height() / 2);
        disUp(true);
        return;
    }

    // NO title in viewer
    disUp(true);
}

// ---- Place lifecycle ----

void initQrApp()
{
    g_loaded = false;
    g_selected = 0;
    g_viewerMode = false;
    renderMenu();
}

void loopQrApp()
{
    buttonState btn = useButton();

    if (!g_viewerMode) {
        if (btn == Up) {
            loadQrListOnce();
            g_selected--;
            if (g_selected < 0) g_selected = (int)g_titles.size() - 1;
            renderMenu();
            delayTask(120);
        } else if (btn == Down) {
            loadQrListOnce();
            g_selected++;
            if (g_selected >= (int)g_titles.size()) g_selected = 0;
            renderMenu();
            delayTask(120);
        } else if (btn == Menu) {
            g_viewerMode = true;
            renderViewer();
            delayTask(120);
        }
    } else {
        // Viewer mode: any press returns to menu
        if (btn != None) {
            g_viewerMode = false;
            renderMenu();
            delayTask(120);
        }
    }

    if (btn != None) {
        resetSleepDelay(SLEEP_EVERY_MS);
    }
}

#endif
