#include "pipboy.h"

#if WATCHFACE_PIPBOY

#include "rtcMem.h"

namespace
{
RTC_DATA_ATTR uint8_t vaultBoyNumber = 0;
RTC_DATA_ATTR uint8_t lastWeatherHour = 255;
RTC_DATA_ATTR bool pipboyDarkMode = true;

constexpr uint16_t PIPBOY_STEPS_GOAL = 5000;

uint16_t backgroundColor()
{
    return pipboyDarkMode ? GxEPD_BLACK : GxEPD_WHITE;
}

uint16_t foregroundColor()
{
    return pipboyDarkMode ? GxEPD_WHITE : GxEPD_BLACK;
}

const GFXfont *font8()
{
    return getFont("pipboy/monofonto8");
}

const GFXfont *font10()
{
    return getFont("pipboy/monofonto10");
}

const GFXfont *font28()
{
    return getFont("pipboy/monofonto28");
}

void markChanged()
{
    dUChange = true;
}

void clearBackground(int16_t x, int16_t y, int16_t w, int16_t h)
{
    dis->fillRect(x, y, w, h, backgroundColor());
    markChanged();
}

void drawNativeImage(int16_t x, int16_t y, const String &name)
{
    writeImageN(x, y, getImg("pipboy/" + name), foregroundColor(), backgroundColor());
    markChanged();
}

void drawNativeImageTransparent(int16_t x, int16_t y, const String &name)
{
    ImageDef *image = getImg("pipboy/" + name);
    dis->drawBitmap(x, y, image->bitmap, image->bw, image->bh, foregroundColor());
    markChanged();
}

bool wifiConfigured()
{
    for (size_t i = 0; i < SIZE_WIFI_CRED_STAT; i++)
    {
        const WiFiCred *credential = wifiCredStatic[i];
        if (credential != nullptr && credential->ssid != nullptr && credential->password != nullptr &&
            strlen(credential->ssid) > 0 && strlen(credential->password) >= 8)
        {
            return true;
        }
    }
    return false;
}

void drawWifi()
{
    drawNativeImageTransparent(120, 77, wifiConfigured() ? "wifi" : "wifioff");
}

void drawStaticFrame()
{
    dis->fillScreen(backgroundColor());
    dis->setTextColor(foregroundColor());
    setTextSize(1);
    setFont(font8());

    drawNativeImage(0, 10, "menubar");
    dis->setCursor(22, 14);
    dis->print("STAT  INV  DATA  MAP");

    dis->setCursor(10, 195);
    dis->print("PIP-BOY 3000 ROBCO IND.");

    dis->drawLine(137, 28, 200, 28, foregroundColor());
    dis->drawLine(137, 28, 137, 132, foregroundColor());
    dis->drawLine(137, 132, 157, 132, foregroundColor());
    dis->drawLine(180, 132, 200, 132, foregroundColor());
    markChanged();
}

void drawVaultBoy()
{
    if (timeRTCLocal.Minute % 15 == 0)
    {
        vaultBoyNumber = random(0, 3);
    }

    clearBackground(58, 48, 76, 102);
    switch (vaultBoyNumber)
    {
    case 0:
        drawNativeImage(70, 50, "vaultboy");
        break;
    case 1:
        drawNativeImage(70, 50, "vaultboypoint");
        break;
    default:
        drawNativeImage(60, 50, "vaultboysmile");
        break;
    }
}

uint8_t displayHour()
{
#if WATCHFACE_12H
    const uint8_t hour = timeRTCLocal.Hour % 12;
    return hour == 0 ? 12 : hour;
#else
    return timeRTCLocal.Hour;
#endif
}

void drawTime()
{
    clearBackground(140, 31, 60, 115);
    dis->setTextColor(foregroundColor());
    setTextSize(1);
    setFont(font28());

    const uint8_t hour = displayHour();
    dis->setCursor(141, 75);
    if (hour < 10)
    {
        dis->print('0');
    }
    dis->print(hour);

    dis->setCursor(141, 125);
    if (timeRTCLocal.Minute < 10)
    {
        dis->print('0');
    }
    dis->print(timeRTCLocal.Minute);

    setFont(font8());
    dis->setCursor(160, 140);
    dis->print(timeRTCLocal.Hour < 11 ? WF_TIME_AM : WF_TIME_PM);
    markChanged();
}

void drawDate()
{
    clearBackground(4, 25, 128, 56);
    dis->setTextColor(foregroundColor());
    setTextSize(1);
    setFont(font10());

    static const char *dayNames[] = LANGUAGE_FULL_DAY_NAMES;
    String dayName = dayNames[(timeRTCLocal.Wday + 6) % 7];
    dayName.toUpperCase();

    int16_t x1, y1;
    uint16_t w, h;
    dis->getTextBounds(dayName, 7, 42, &x1, &y1, &w, &h);
    dis->fillRect(x1 - 2, y1 - 2, w + 4, h + 4, foregroundColor());
    dis->setTextColor(backgroundColor());
    dis->setCursor(7, 42);
    dis->print(dayName);

    dis->setTextColor(foregroundColor());
    dis->setCursor(7, 62);
    String month = getLocalizedMonthName(timeRTCLocal.Month);
    month.toUpperCase();
    dis->print(month);
    dis->print(' ');
    dis->print(timeRTCLocal.Day);
    dis->setCursor(7, 78);
    dis->print(tmYearToCalendar(timeRTCLocal.Year));
    markChanged();
}

void drawSteps()
{
    clearBackground(58, 151, 142, 28);
    const uint16_t stepCount = getSteps();
    const uint32_t progressRaw = (static_cast<uint32_t>(stepCount) * 100UL) / PIPBOY_STEPS_GOAL;
    const uint8_t progress = progressRaw > 100 ? 100 : static_cast<uint8_t>(progressRaw);

    drawNativeImage(60, 155, "gauge");
    dis->fillRect(73, 160, (progress / 2) + 5, 4, foregroundColor());

    dis->setTextColor(foregroundColor());
    setTextSize(1);
    setFont(font8());
    dis->setCursor(150, 160);
    dis->print(WF_PIPBOY_STEPS);
    dis->setCursor(150, 175);
    dis->print(stepCount);
    markChanged();
}

void drawBattery()
{
    clearBackground(8, 148, 41, 26);
    drawNativeImage(10, 150, "battery");
    dis->fillRect(15, 155, 27, 11, backgroundColor());

    uint8_t segments = 0;
    if (rM.bat.curV > 4.10f)
    {
        segments = 3;
    }
    else if (rM.bat.curV > 3.95f)
    {
        segments = 2;
    }
    else if (rM.bat.curV > 3.80f)
    {
        segments = 1;
    }

    for (uint8_t segment = 0; segment < segments; segment++)
    {
        dis->fillRect(15 + (segment * 9), 155, 7, 11, foregroundColor());
    }
    markChanged();
}

#if WEATHER_INFO
const char *weatherIcon(uint8_t code)
{
    if (code == 0)
        return "sunny";
    if (code <= 2)
        return "cloudsun";
    if (code == 3)
        return "cloudy";
    if (code == 45 || code == 48)
        return "atmosphere";
    if (code >= 51 && code <= 57)
        return "drizzle";
    if ((code >= 61 && code <= 67) || (code >= 80 && code <= 82))
        return "rain";
    if ((code >= 71 && code <= 77) || code == 85 || code == 86)
        return "snow";
    if (code >= 95)
        return "thunderstorm";
    return nullptr;
}
#endif

void drawWeather(bool force)
{
#if WEATHER_INFO
    if (!force && lastWeatherHour == timeRTCLocal.Hour)
    {
        return;
    }
    lastWeatherHour = timeRTCLocal.Hour;
#else
    if (!force)
    {
        return;
    }
#endif

    clearBackground(5, 84, 50, 53);
    dis->setTextColor(foregroundColor());
    setTextSize(1);
    setFont(font10());
    dis->setCursor(12, 133);

#if WEATHER_INFO
    const OM_OneHourWeather weather = weatherGetDataHourly(WEATHER_WATCHFACE_HOUR_OFFSET);
    if (weather.fine)
    {
        const char *icon = weatherIcon(weather.weather_code);
        if (icon != nullptr)
        {
            drawNativeImage(5, 85, icon);
        }
        dis->print(formatTemperature(weather.temp));
    }
    else
#endif
    {
        dis->print("--");
    }
    markChanged();
}

void drawTimeBeforeApply()
{
    drawVaultBoy();
    drawTime();
}

void drawTimeAfterApply(bool forceDraw)
{
    drawSteps();
    drawWeather(forceDraw);
    drawWifi();
}

void showTimeFull()
{
    drawVaultBoy();
    drawTime();
}

void initWatchface()
{
    lastWeatherHour = 255;
    drawStaticFrame();
}

void setDarkMode(bool enabled)
{
    if (pipboyDarkMode == enabled)
    {
        return;
    }

    pipboyDarkMode = enabled;
    initWatchface();
    showTimeFull();
    drawDate();
    drawSteps();
    drawWeather(true);
    drawBattery();
    drawWifi();
    rM.updateCounter = FULL_DISPLAY_UPDATE_QUEUE;
    markChanged();
}

void manageInput(buttonState button)
{
    if (button != None)
    {
        resetSleepDelay(SLEEP_EVERY_MS);
    }

    switch (button)
    {
    case Up:
        setDarkMode(false);
        break;
    case Down:
        setDarkMode(true);
        break;
    case Menu:
        generalSwitch(mainMenu);
        break;
#if LONG_BACK_FULL_REFRESH
    case LongBack:
        rM.updateCounter = FULL_DISPLAY_UPDATE_QUEUE;
        markChanged();
        break;
#endif
    default:
        break;
    }
}
} // namespace

const watchfaceDefOne pipboyDef = {
    .drawTimeBeforeApply = drawTimeBeforeApply,
    .drawTimeAfterApply = drawTimeAfterApply,
    .drawDay = drawDate,
    .drawMonth = []() {},
    .showTimeFull = showTimeFull,
    .initWatchface = initWatchface,
    .drawBattery = drawBattery,
    .manageInput = manageInput,
    .watchfaceModules = false,
    .watchfaceModSquare = {.size{.w = 0, .h = 0}, .cord{.x = 0, .y = 0}},
    .someDrawingSquare = {.size{.w = 200, .h = 200}, .cord{.x = 0, .y = 0}},
    .isModuleEngaged = []() { return false; },
    .lpCoreScreenPrepareCustom = NULL,
};

#endif
