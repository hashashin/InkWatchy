#include "worldClock.h"

#if WORLD_CLOCK_APP

#include "rtcMem.h"
#include <stdint.h>
#include <stdio.h>

#define WORLD_CLOCK_FONT_TITLE getFont("dogicapixel4")
#define WORLD_CLOCK_FONT_TIME  getFont("UbuntuMono10")

#define WORLD_CLOCK_TILES_PER_PAGE 4
#define WORLD_CLOCK_TILE_W 94
#define WORLD_CLOCK_TILE_H 68

#ifndef WORLD_CLOCK_TILES
#define WORLD_CLOCK_TILES(X) \
    X("UTC", 0)              \
    X("MAD", 120)            \
    X("NYC", -240)           \
    X("TOK", 540)
#endif

struct WorldClockTile
{
    const char* label;
    int16_t offsetMinutes;
};

#define WORLD_CLOCK_ROW(label, offset) {label, offset},
static const WorldClockTile g_tiles[] = {
    WORLD_CLOCK_TILES(WORLD_CLOCK_ROW)
};
#undef WORLD_CLOCK_ROW

static uint8_t g_page = 0;
static uint8_t g_lastMinute = 255;
static bool g_dirty = true;

static uint8_t tileCount()
{
    return sizeof(g_tiles) / sizeof(g_tiles[0]);
}

static uint8_t pageCount()
{
    uint8_t count = tileCount();
    return (count + WORLD_CLOCK_TILES_PER_PAGE - 1) / WORLD_CLOCK_TILES_PER_PAGE;
}

static String offsetLabel(int16_t offsetMinutes)
{
    char buf[8];
    char sign = offsetMinutes >= 0 ? '+' : '-';
    int16_t absMinutes = offsetMinutes >= 0 ? offsetMinutes : -offsetMinutes;
    if (absMinutes % 60 == 0) {
        snprintf(buf, sizeof(buf), "%c%d", sign, absMinutes / 60);
    } else {
        snprintf(buf, sizeof(buf), "%c%d:%02d", sign, absMinutes / 60, absMinutes % 60);
    }
    return String(buf);
}

static int64_t unixForOffset(int16_t offsetMinutes)
{
    return (int64_t)getUnixTime(timeRTCUTC0) + ((int64_t)offsetMinutes * 60);
}

static String shortDate(int64_t unixTime)
{
    tmElements_t tmEl;
    rM.SRTC.doBreakTime(unixTime, tmEl);

    char buf[6];
    snprintf(buf, sizeof(buf), "%02d.%02d", tmEl.Day, tmEl.Month);
    return String(buf);
}

static void renderTile(uint8_t index, int x, int y)
{
    const WorldClockTile& tile = g_tiles[index];

    dis->drawRect(x, y, WORLD_CLOCK_TILE_W, WORLD_CLOCK_TILE_H, SCBlack);
    dis->setFont(WORLD_CLOCK_FONT_TITLE);
    dis->setCursor(x + 5, y + 11);
    dis->print(tile.label);
    dis->setCursor(x + 64, y + 11);
    dis->print(offsetLabel(tile.offsetMinutes));

    int64_t tileUnix = unixForOffset(tile.offsetMinutes);
    dis->setFont(WORLD_CLOCK_FONT_TIME);
    dis->setCursor(x + 11, y + 38);
    dis->print(getHourMinuteUnix(tileUnix));
    dis->setCursor(x + 11, y + 57);
    dis->print(shortDate(tileUnix));
}

static void renderWorldClock()
{
    readRTC();

    dis->fillScreen(SCWhite);
    dis->setTextColor(SCBlack);
    dis->setFont(WORLD_CLOCK_FONT_TITLE);
    dis->setCursor(8, 14);
    dis->print(WORLD_CLOCK_TITLE);

    if (tileCount() == 0) {
        dis->setFont(WORLD_CLOCK_FONT_TIME);
        dis->setCursor(12, 72);
        dis->print(WORLD_CLOCK_EMPTY);
        dUChange = true;
        return;
    }

    uint8_t start = g_page * WORLD_CLOCK_TILES_PER_PAGE;
    for (uint8_t i = 0; i < WORLD_CLOCK_TILES_PER_PAGE && start + i < tileCount(); i++) {
        int col = i % 2;
        int row = i / 2;
        renderTile(start + i, 4 + col * 100, 24 + row * 74);
    }

    dis->setFont(WORLD_CLOCK_FONT_TITLE);
    dis->setCursor(8, 194);
    dis->print(WORLD_CLOCK_HINT);
    if (pageCount() > 1) {
        dis->setCursor(154, 194);
        dis->print(String(g_page + 1) + "/" + String(pageCount()));
    }

    dUChange = true;
}

void initWorldClock()
{
    g_page = 0;
    g_lastMinute = 255;
    g_dirty = true;
    renderWorldClock();
    disUp(true);
}

void loopWorldClock()
{
    buttonState btn = useButton();
    resetSleepDelay(SLEEP_EVERY_MS);

    if (btn == Up && pageCount() > 1) {
        g_page = (g_page + pageCount() - 1) % pageCount();
        g_dirty = true;
    } else if (btn == Down && pageCount() > 1) {
        g_page = (g_page + 1) % pageCount();
        g_dirty = true;
    }

    readRTC();
    if (g_lastMinute != timeRTCLocal.Minute) {
        g_lastMinute = timeRTCLocal.Minute;
        g_dirty = true;
    }

    if (g_dirty) {
        g_dirty = false;
        renderWorldClock();
    }

    disUp(false);
}

void exitWorldClock()
{
}

#endif
