#include "binWatch.h"

#if WATCHFACE_BINWATCH

#include "rtcMem.h"

// ------------------------------------------------------------
// Cyberpunk Brutal (daily usable) - Pure binary time + binary date
// HH: 0-23 (5 bits)   MM: 0-59 (6 bits)
// DAY: 1-31 (5 bits)  MON: 1-12 (4 bits)
// Refresh model: wfmOne -> minute-based, RTC can be slowed at night via NIGHT_SLEEP_*
// ------------------------------------------------------------

#define LABEL_FONT getFont("UbuntuMono10")

// Layout
static constexpr int kW = 200;
static constexpr int kPadX = 12;

static constexpr int kBit = 14;       // square size
static constexpr int kBitGap = 6;     // gap between squares
static constexpr int kRowGap = 14;    // gap between hour/min rows

static constexpr int kTopY = 28;      // top HUD
static constexpr int kHourY = 52;     // hour bits row y
static constexpr int kMinY  = kHourY + kBit + kRowGap; // min bits row y

static constexpr int kDateY = 128;    // << SUBIDO (antes 136)
static constexpr int kBatY  = 176;    // battery hud y
static constexpr int kSepY  = (kMinY + kBit + kDateY) / 2; // separator line y

// Battery bar
static constexpr int kBatBarW = 62;
static constexpr int kBatBarH = 10;
static constexpr int kBatBarX = kW - kPadX - kBatBarW;

// Night low-refresh indicator area
static constexpr int kLRX = 132;
static constexpr int kLRY = kTopY;
static constexpr int kLRW = 56;
static constexpr int kLRH = 14;

static bool g_lastNight = false;

static inline bool isNightLowRefresh()
{
#if defined(NIGHT_SLEEP_AFTER_HOUR) && defined(NIGHT_SLEEP_BEFORE_HOUR) && defined(NIGHT_SLEEP_FOR_M)
    if (NIGHT_SLEEP_FOR_M == 1) return false; // disabled
    int h = timeRTCLocal.Hour;
    return (h >= NIGHT_SLEEP_AFTER_HOUR || h < NIGHT_SLEEP_BEFORE_HOUR);
#else
    // sensible default if not defined
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
}

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
}

static void drawHudStatic()
{
    dis->setTextColor(SCBlack);
    dis->setFont(LABEL_FONT);

    // Top header
    dis->setCursor(kPadX, kTopY);
    dis->print("SYS TIME");

    // Thin HUD line top
    dis->drawFastHLine(kPadX, kTopY + 10, kW - kPadX * 2, SCBlack);

    // Labels left
    dis->setCursor(kPadX, kHourY + 12);
    dis->print("H");
    dis->setCursor(kPadX, kMinY + 12);
    dis->print("M");

    // Separator line before date
    dis->drawFastHLine(kPadX, kSepY, kW - kPadX * 2, SCBlack);

    // Bottom fixed labels
    dis->setCursor(kPadX, kBatY + 10);
    dis->print("SYS OK");
    dis->setCursor(kBatBarX - 34, kBatY + 10);
    dis->print("BAT");

    // Night low-refresh indicator (optional)
    drawLowRefreshIndicator(true);
}

static void drawBitsRow(int x, int y, int bits, uint32_t value)
{
    // MSB left -> LSB right
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

static void drawTimePureBinary(bool full)
{
    const int h = timeRTCLocal.Hour;    // 0-23
    const int m = timeRTCLocal.Minute;  // 0-59

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

    // Colon vertical centrado entre filas (H y M)
    const int dot = 3;
    const int xColon = (kW / 2) - (dot / 2);

    const int yTopRowEnd = kHourY + kBit;
    const int yBottomRowStart = kMinY;
    const int yMid = (yTopRowEnd + yBottomRowStart) / 2;

    dis->fillRect(xColon, yMid - 6, dot, dot, SCBlack);
    dis->fillRect(xColon, yMid + 4, dot, dot, SCBlack);
}

static void drawDateBinary(bool full)
{
    const int day = timeRTCLocal.Day;     // 1-31
    const int mon = timeRTCLocal.Month;   // 1-12

    const int dayBits = 5;
    const int monBits = 4;

    // Dos filas (DAY arriba, MONTH abajo)
    const int dayY = kDateY;
    const int monY = kDateY + (kBit + 10);

    // limpiamos area date completa, pero SIN invadir la zona de bateria
    (void)full;
    {
        const int y0 = dayY - 2;
        int h = (kBatY - 2) - y0;      // hasta justo antes de BAT/SYS OK
        if (h < (kBit * 2) + 18) {
            // fallback minimo por si alguien mueve kBatY/kDateY raro
            h = (kBit * 2) + 18;
        }
        clearRect(0, y0, kW, h);
    }

    dis->setTextColor(SCBlack);
    dis->setFont(LABEL_FONT);

    // medir ancho real de tus labels localizados
    int16_t tbx, tby;
    uint16_t twDay, thDay, twMon, thMon;
    dis->getTextBounds(TIME_UNIT_DAY_B, 0, 0, &tbx, &tby, &twDay, &thDay);
    dis->getTextBounds(TIME_UNIT_MONTH, 0, 0, &tbx, &tby, &twMon, &thMon);

    const int gapLabelBits = 6;

    // ---- fila DIA ----
    {
        const int rowW = (int)twDay + gapLabelBits + bitsWidth(dayBits);
        int x = (kW - rowW) / 2;

        dis->setCursor(x, dayY + 12);
        dis->print(TIME_UNIT_DAY_B);

        int bitsX = x + (int)twDay + gapLabelBits;
        drawBitsRow(bitsX, dayY, dayBits, (uint32_t)day);
    }

    // ---- fila MES ----
    {
        const int rowW = (int)twMon + gapLabelBits + bitsWidth(monBits);
        int x = (kW - rowW) / 2;

        dis->setCursor(x, monY + 12);
        dis->print(TIME_UNIT_MONTH);

        int bitsX = x + (int)twMon + gapLabelBits;
        drawBitsRow(bitsX, monY, monBits, (uint32_t)mon);
    }
}

static void drawBatteryHud()
{
    clearRect(kBatBarX - 40, kBatY - 2, 40 + kBatBarW + 2, kBatBarH + 14);

    dis->setTextColor(SCBlack);
    dis->setFont(LABEL_FONT);

    dis->setCursor(kBatBarX - 34, kBatY + 10);
    dis->print("BAT");

    drawProgressBar(kBatBarX, kBatY, kBatBarW, kBatBarH, rM.batteryPercantageWF);
}

// ------------------------------------------------------------
// watchfaceDefOne hooks
// ------------------------------------------------------------

static void drawTimeBeforeApply()
{
    // If night mode boundary crossed, refresh LR indicator
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
        updateDisplay(FULL_UPDATE);
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
