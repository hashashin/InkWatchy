#include "binWatch.h"

#if WATCHFACE_BINWATCH

#include "rtcMem.h"

#define LABEL_FONT getFont("UbuntuMono10")

// Layout
static constexpr int kW = 200;
static constexpr int kPadX = 12;

static constexpr int kBit = 14;
static constexpr int kBitGap = 6;
static constexpr int kRowGap = 14;

static constexpr int kTopY  = 28;
static constexpr int kHourY = 52;
static constexpr int kMinY  = kHourY + kBit + kRowGap;

static constexpr int kDateY = 128;
static constexpr int kBatY  = 176;
static constexpr int kSepY  = (kMinY + kBit + kDateY) / 2;

// Battery bar
static constexpr int kBatBarW = 62;
static constexpr int kBatBarH = 10;
static constexpr int kBatBarX = kW - kPadX - kBatBarW;

// Low refresh indicator
static constexpr int kLRX = 132;
static constexpr int kLRY = kTopY;
static constexpr int kLRW = 56;
static constexpr int kLRH = 14;

static bool g_lastNight = false;

// Refresh (core queue)
static inline void requestCoreFullRefresh()
{
    dUChange = true;
    rM.updateCounter = FULL_DISPLAY_UPDATE_QUEUE;
}

static inline bool isNightLowRefresh()
{
#if defined(NIGHT_SLEEP_AFTER_HOUR) && defined(NIGHT_SLEEP_BEFORE_HOUR) && defined(NIGHT_SLEEP_FOR_M)
    if (NIGHT_SLEEP_FOR_M == 1) return false;
    int h = timeRTCLocal.Hour;
    return (h >= NIGHT_SLEEP_AFTER_HOUR || h < NIGHT_SLEEP_BEFORE_HOUR);
#else
    int h = timeRTCLocal.Hour;
    return (h >= 23 || h < 6);
#endif
}

// Helpers
static inline int bitsWidth(int bits)
{
    return bits * kBit + (bits - 1) * kBitGap;
}

static void clearRect(int x, int y, int w, int h)
{
    dis->fillRect(x, y, w, h, SCWhite);
    dUChange = true;
}

// HUD: low refresh
static void drawLowRefreshIndicator(bool force)
{
    bool night = isNightLowRefresh();
    if (!force && night == g_lastNight) return;

    g_lastNight = night;

    clearRect(kLRX, kLRY - 12, kLRW, kLRH + 12);

    if (night) {
        dis->setTextColor(SCBlack);
        dis->setFont(LABEL_FONT);
        dis->setCursor(kLRX, kLRY);
#if defined(NIGHT_SLEEP_FOR_M)
        dis->print("LR ");
        dis->print(NIGHT_SLEEP_FOR_M);
        dis->print("m");
#else
        dis->print("LR 45m");
#endif
    }

    dUChange = true;
}

// HUD: static frame
static void drawHudStatic()
{
    dis->setTextColor(SCBlack);
    dis->setFont(LABEL_FONT);

    dis->setCursor(kPadX, kTopY);
    dis->print("SYS TIME");

    dis->drawFastHLine(kPadX, kTopY + 10, kW - kPadX * 2, SCBlack);

    dis->setCursor(kPadX, kHourY + 12);
    dis->print("H");
    dis->setCursor(kPadX, kMinY + 12);
    dis->print("M");

    dis->drawFastHLine(kPadX, kSepY, kW - kPadX * 2, SCBlack);

    dis->setCursor(kPadX, kBatY + 10);
    dis->print("SYS OK");
    dis->setCursor(kBatBarX - 34, kBatY + 10);
    dis->print("BAT");

    drawLowRefreshIndicator(true);

    dUChange = true;
}

// Bits rows
static void drawBitsRow(int x, int y, int bits, uint32_t value)
{
    for (int i = 0; i < bits; i++) {
        int bitIndex = bits - 1 - i;
        bool on = (value & (1u << bitIndex)) != 0;

        int bx = x + i * (kBit + kBitGap);

        if (on) {
            dis->fillRect(bx, y, kBit, kBit, SCBlack);
        } else {
            dis->drawRect(bx, y, kBit, kBit, SCBlack);
        }
    }
}

// Time
static void drawTimePureBinary(bool full)
{
    const int h = timeRTCLocal.Hour;
    const int m = timeRTCLocal.Minute;

    const int hourBits = 5;
    const int minBits  = 6;

    const int hourX = (kW - bitsWidth(hourBits)) / 2;
    const int minX  = (kW - bitsWidth(minBits)) / 2;

    if (full) {
        clearRect(0, kHourY - 2, kW, (kMinY - kHourY) + kBit + 4);
    } else {
        clearRect(hourX, kHourY, bitsWidth(hourBits), kBit);
        clearRect(minX,  kMinY,  bitsWidth(minBits),  kBit);
    }

    drawBitsRow(hourX, kHourY, hourBits, (uint32_t)h);
    drawBitsRow(minX,  kMinY,  minBits,  (uint32_t)m);

    const int dot = 3;
    const int xColon = (kW / 2) - (dot / 2);

    const int yTopRowEnd = kHourY + kBit;
    const int yBottomRowStart = kMinY;
    const int yMid = (yTopRowEnd + yBottomRowStart) / 2;

    dis->fillRect(xColon, yMid - 6, dot, dot, SCBlack);
    dis->fillRect(xColon, yMid + 4, dot, dot, SCBlack);

    dUChange = true;
}

// Date
static void drawDateBinary(bool full)
{
    const int day = timeRTCLocal.Day;
    const int mon = timeRTCLocal.Month;

    const int dayBits = 5;
    const int monBits = 4;

    const int dayY = kDateY;
    const int monY = kDateY + (kBit + 10);

    (void)full;
    {
        const int y0 = dayY - 2;
        int h = (kBatY - 2) - y0;
        if (h < (kBit * 2) + 18) {
            h = (kBit * 2) + 18;
        }
        clearRect(0, y0, kW, h);
    }

    dis->setTextColor(SCBlack);
    dis->setFont(LABEL_FONT);

    int16_t tbx, tby;
    uint16_t twDay, thDay, twMon, thMon;
    dis->getTextBounds(TIME_UNIT_DAY_B, 0, 0, &tbx, &tby, &twDay, &thDay);
    dis->getTextBounds(TIME_UNIT_MONTH, 0, 0, &tbx, &tby, &twMon, &thMon);

    const int gapLabelBits = 6;

    {
        const int rowW = (int)twDay + gapLabelBits + bitsWidth(dayBits);
        int x = (kW - rowW) / 2;

        dis->setCursor(x, dayY + 12);
        dis->print(TIME_UNIT_DAY_B);

        int bitsX = x + (int)twDay + gapLabelBits;
        drawBitsRow(bitsX, dayY, dayBits, (uint32_t)day);
    }

    {
        const int rowW = (int)twMon + gapLabelBits + bitsWidth(monBits);
        int x = (kW - rowW) / 2;

        dis->setCursor(x, monY + 12);
        dis->print(TIME_UNIT_MONTH);

        int bitsX = x + (int)twMon + gapLabelBits;
        drawBitsRow(bitsX, monY, monBits, (uint32_t)mon);
    }

    dUChange = true;
}

// Battery
static void drawBatteryHud()
{
    clearRect(kBatBarX - 40, kBatY - 2, 40 + kBatBarW + 2, kBatBarH + 14);

    dis->setTextColor(SCBlack);
    dis->setFont(LABEL_FONT);

    dis->setCursor(kBatBarX - 34, kBatY + 10);
    dis->print("BAT");

    drawProgressBar(kBatBarX, kBatY, kBatBarW, kBatBarH, rM.batteryPercantageWF);

    dUChange = true;
}

// watchfaceDefOne hooks
static void drawTimeBeforeApply()
{
    drawLowRefreshIndicator(false);

    const bool hourChanged = (rM.wFTime.Hour   != timeRTCLocal.Hour);
    const bool minChanged  = (rM.wFTime.Minute != timeRTCLocal.Minute);

    if (hourChanged || minChanged) {
        drawTimePureBinary(false);
    }
}

static void drawTimeAfterApply(bool /*forceDraw*/) {}

static void drawDay()   { drawDateBinary(false); }
static void drawMonth() { drawDateBinary(false); }

static void showTimeFull() { drawTimePureBinary(true); }

static void initWatchface()
{
    dis->fillScreen(SCWhite);
    dUChange = true;

    drawHudStatic();
    drawTimePureBinary(true);
    drawDateBinary(true);
    drawBatteryHud();
}

static void drawBattery() { drawBatteryHud(); }

static void manageInput(buttonState bt)
{
    if (bt != None) {
        resetSleepDelay(SLEEP_EVERY_MS);
    }

    switch (bt) {
    case Menu:
        generalSwitch(mainMenu);
        break;
#if LONG_BACK_FULL_REFRESH
    case LongBack:
        requestCoreFullRefresh();
        break;
#endif
    default:
        break;
    }
}

const watchfaceDefOne binWatchDef = {
    .drawTimeBeforeApply = drawTimeBeforeApply,
    .drawTimeAfterApply  = drawTimeAfterApply,
    .drawDay             = drawDay,
    .drawMonth           = drawMonth,
    .showTimeFull        = showTimeFull,
    .initWatchface       = initWatchface,
    .drawBattery         = drawBattery,
    .manageInput         = manageInput,

    .watchfaceModules = false,
    .watchfaceModSquare = {.size{.w = 0, .h = 0}, .cord{.x = 0, .y = 0}},
    .someDrawingSquare  = {.size{.w = 0, .h = 0}, .cord{.x = 0, .y = 0}},
    .isModuleEngaged = []() { return false; },
    .lpCoreScreenPrepareCustom = NULL,
};

#endif
