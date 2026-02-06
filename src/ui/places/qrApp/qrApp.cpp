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
static int g_scroll = 0;

// Parsed entries
static std::vector<std::string> g_titles;
static std::vector<std::string> g_payloads;

enum QrType {
    QR_WEB,
    QR_WIFI,
    QR_TEL,
    QR_CONTACT,
    QR_TEXT
};

static std::vector<QrType> g_types;

static void trimLine(std::string& s)
{
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ' || s.back() == '\t')) {
        s.pop_back();
    }
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) i++;
    if (i > 0) s.erase(0, i);
}

static QrType detectType(const std::string& payload)
{
    if (payload.rfind("WIFI:",0)==0) return QR_WIFI;
    if (payload.rfind("tel:",0)==0) return QR_TEL;
    if (payload.rfind("BEGIN:VCARD",0)==0) return QR_CONTACT;
    if (payload.rfind("http",0)==0) return QR_WEB;
    return QR_TEXT;
}

static void pushEntryFromLine(const std::string& lineIn)
{
    std::string line = lineIn;
    trimLine(line);
    if (line.empty()) return;
    if (line[0] == '#') return;

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
    g_types.push_back(detectType(payload));
}

static void loadQrListOnce()
{
    if (g_loaded) return;
    g_loaded = true;

    g_titles.clear();
    g_payloads.clear();
    g_types.clear();

    if (!LittleFS.exists(kQrListPath)) {
        g_titles.emplace_back("Missing qrlist.txt");
        g_payloads.emplace_back("Missing qrlist.txt");
        g_types.emplace_back(QR_TEXT);
        return;
    }

    File f = LittleFS.open(kQrListPath, "r");
    if (!f) {
        g_titles.emplace_back("Open qrlist failed");
        g_payloads.emplace_back("Open qrlist failed");
        g_types.emplace_back(QR_TEXT);
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
        g_types.emplace_back(QR_TEXT);
    }

    if (g_selected < 0 || g_selected >= (int)g_titles.size()) g_selected = 0;
}

// ---- ICON DRAW ----

static ImageDef* getIcon(QrType t)
{
    switch(t){
        case QR_WIFI: return getImg("qrapp/wifi");
        case QR_TEL: return getImg("qrapp/tel");
        case QR_CONTACT: return getImg("qrapp/contact");
        case QR_WEB: return getImg("qrapp/web");
        default: return getImg("qrapp/text");
    }
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
    const int headerY = 12;
    const int headerPadBottom = 6;

    String headerStr = "QR LIST";
    uint16_t tw, th;
    getTextBounds(headerStr, NULL, NULL, &tw, &th);

    int headerX = (dis->width() - (int)tw) / 2;
    if (headerX < 0) headerX = 0;

    dis->setCursor(headerX, headerY);
    dis->print(headerStr);

    const int listTop = headerY + headerPadBottom + 8;
    const int listBottom = dis->height() - 14;

    int visibleRows = (listBottom - listTop) / lineH;
    if (visibleRows < 1) visibleRows = 1;

    if (g_selected < g_scroll)
        g_scroll = g_selected;

    if (g_selected >= g_scroll + visibleRows)
        g_scroll = g_selected - visibleRows + 1;

    if (g_scroll < 0) g_scroll = 0;
    if (g_scroll > (int)g_titles.size() - visibleRows)
        g_scroll = std::max(0, (int)g_titles.size() - visibleRows);

    int y = listTop;

    for (int i = 0; i < visibleRows; i++)
    {
        int idx = g_scroll + i;
        if (idx >= (int)g_titles.size()) break;

        if (idx == g_selected) {
            dis->fillRect(0, y - (lineH - 2), dis->width(), lineH, SCBlack);
            dis->setTextColor(SCWhite);
        } else {
            dis->setTextColor(SCBlack);
        }

        // icono
        ImageDef* img = getIcon(g_types[idx]);
        if (img && img->bitmap)
            writeImageN(4, y-12, img);

        // texto
        dis->setCursor(26, y);
        dis->print(g_titles[idx].c_str());

        y += lineH;
    }

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

    if (esp_qrcode_generate(&cfg, txt) != ESP_OK) {
        writeTextCenterReplaceBack("QR error", dis->height() / 2);
        disUp(true);
        return;
    }

    disUp(true);
}

// ---- lifecycle ----

void initQrApp()
{
    g_loaded = false;
    g_selected = 0;
    g_scroll = 0;
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
        } 
        else if (btn == Down) {
            loadQrListOnce();
            g_selected++;
            if (g_selected >= (int)g_titles.size()) g_selected = 0;
            renderMenu();
            delayTask(120);
        } 
        else if (btn == Menu) {
            g_viewerMode = true;
            renderViewer();
            delayTask(120);
        }
    } else {
        if (btn != None) {
            g_viewerMode = false;
            renderMenu();
            delayTask(120);
        }
    }

    if (btn != None)
        resetSleepDelay(SLEEP_EVERY_MS);
}

#endif
