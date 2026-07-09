#include "stopWatch.h"

#if STOPWATCH

#include "rtcMem.h"

#define SW_FONT_BIG   getFont("inkfield/JackInput40")
#define SW_FONT_SMALL getFont("dogicapixel4")
#define SW_FONT_MONO  getFont("UbuntuMono10")

static bool g_running = false;
static uint32_t g_startMs = 0;
static uint32_t g_accumMs = 0;

static uint32_t g_lastUiMs = 0;
static uint32_t g_lastDrawMs = 0;

static const size_t kMaxLaps = 16;
static uint32_t g_laps[kMaxLaps] = {};
static size_t g_lapCount = 0;

// Simple caches to avoid redrawing unchanged text
static String g_lastTime;
static String g_lastInfo;
static String g_lastLaps[4];

static inline void invalidateAll()
{
    // Sentinel values (not empty) so the next render clears old content
    g_lastTime = "\x01";
    g_lastInfo = "\x01";
    for (int i = 0; i < 4; i++) g_lastLaps[i] = "\x01";
}

static inline void invalidateLaps()
{
    for (int i = 0; i < 4; i++) g_lastLaps[i] = "\x01";
}

static uint32_t elapsedMs()
{
    uint32_t e = g_accumMs;
    if (g_running) {
        e += (millis() - g_startMs);
    }
    return e;
}

// Full format for laps: M:SS.CC
static void fmtTime(uint32_t ms, char* out, size_t outSz)
{
    uint32_t cs = (ms / 10) % 100;
    uint32_t s  = (ms / 1000) % 60;
    uint32_t m  = (ms / 60000);
    snprintf(out, outSz, "%lu:%02lu.%02lu",
             (unsigned long)m, (unsigned long)s, (unsigned long)cs);
}

// Main screen format (no centiseconds): MM:SS
static void fmtTimeMain(uint32_t ms, char* out, size_t outSz)
{
    uint32_t s = (ms / 1000) % 60;
    uint32_t m = (ms / 60000);
    snprintf(out, outSz, "%02lu:%02lu",
             (unsigned long)m, (unsigned long)s);
}

static void drawTime(bool force)
{
    char mainBuf[16];
    fmtTimeMain(elapsedMs(), mainBuf, sizeof(mainBuf));
    String mainT = String(mainBuf);

    if (!force && mainT == g_lastTime) return;

    dis->fillRect(0, 24, 200, 60, SCWhite);
    dis->setTextColor(SCBlack);

    int16_t x1, y1;
    uint16_t w, h;
    dis->setFont(SW_FONT_BIG);
    dis->getTextBounds(mainT, 0, 0, &x1, &y1, &w, &h);

    int16_t x = (int16_t)((200 - (int)w) / 2) - x1;
    if (x < 0) x = 0;

    dis->setFont(SW_FONT_BIG);
    dis->setCursor(x, 70);
    dis->print(mainT);

    g_lastTime = mainT;
    dUChange = true;
}

static void drawInfo(bool force)
{
    String info;

    if (g_running) {
        info = String(STOPWATCH_HINT_MENU) + " " + String(STOPWATCH_HINT_PAUSE) + "  " +
               String(STOPWATCH_HINT_DOWN) + " " + String(STOPWATCH_HINT_LAP);
    } else {
        info = String(STOPWATCH_HINT_MENU) + " " + String(STOPWATCH_HINT_START) + "  " +
               String(STOPWATCH_HINT_BACK) + " " + String(STOPWATCH_HINT_RESET);
    }

    if (!force && info == g_lastInfo) return;

    dis->fillRect(0, 82, 200, 18, SCWhite);
    dis->setFont(SW_FONT_SMALL);
    dis->setTextColor(SCBlack);
    dis->setCursor(12, 95);
    dis->print(info);

    g_lastInfo = info;
    dUChange = true;
}

static void drawLaps(bool force)
{
    dis->setFont(SW_FONT_MONO);
    dis->setTextColor(SCBlack);

    for (int i = 0; i < 4; i++) {
        String line = "";
        int idx = (int)g_lapCount - 1 - i;

        if (idx >= 0) {
            char tb[24];
            fmtTime(g_laps[(size_t)idx], tb, sizeof(tb));
            line = String(STOPWATCH_LAP_LABEL) + " " + String(idx + 1) + " " + String(tb);
        }

        if (!force && line == g_lastLaps[i]) continue;

        int y = 118 + i * 18;
        dis->fillRect(0, y - 16, 200, 22, SCWhite);
        dis->setCursor(12, y);
        dis->print(line);

        g_lastLaps[i] = line;
        dUChange = true;
    }
}

static void render(bool force)
{
    if (force) {
        dis->fillScreen(SCWhite);
        dis->setFont(SW_FONT_SMALL);
        dis->setTextColor(SCBlack);
        dis->setCursor(12, 18);
        dis->print(STOPWATCH_TITLE);
        dUChange = true;
    }

    drawTime(force);
    drawInfo(force);
    drawLaps(force);
}

static void onStartPause()
{
    if (!g_running) {
        g_running = true;
        g_startMs = millis();
    } else {
        g_accumMs += (millis() - g_startMs);
        g_running = false;
    }

    g_lastInfo = "\x01";
    g_lastTime = "\x01";
}

static void onLap()
{
    if (!g_running) return;
    if (g_lapCount >= kMaxLaps) return;

    g_laps[g_lapCount++] = elapsedMs();
    invalidateLaps();
}

static void onReset()
{
    if (g_running) return;

    g_accumMs = 0;
    g_startMs = millis();
    g_lapCount = 0;

    invalidateAll();
}

void initStopwatch()
{
    g_running = false;
    g_startMs = 0;
    g_accumMs = 0;
    g_lastUiMs = millis();
    g_lastDrawMs = 0;

    g_lapCount = 0;

    g_lastTime = "";
    g_lastInfo = "";
    for (int i = 0; i < 4; i++) g_lastLaps[i] = "";

    render(true);
    disUp(true);
}

void loopStopwatch()
{
    buttonState btn = useButton();

    resetSleepDelay(SLEEP_EVERY_MS);

    if (btn == Menu) {
        onStartPause();
    } else if (btn == Down) {
        onLap();
    } else if (btn == Up) {
        onReset();
    }

    uint32_t now = millis();
    uint32_t period = g_running ? 200 : 1000;

    if (g_lastDrawMs == 0 || (now - g_lastDrawMs) >= period) {
        g_lastDrawMs = now;

        render(false);

        bool ignoreCounter = g_running;
        disUp(false, ignoreCounter, false);
    } else {
        disUp(false, false, false);
    }
}

void exitStopwatch()
{
}

#endif
