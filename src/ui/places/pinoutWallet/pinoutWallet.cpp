#include "pinoutWallet.h"

#if PINOUT_WALLET_APP

#include "rtcMem.h"
#include <stdint.h>

#define PINOUT_FONT_TITLE getFont("dogicapixel4")
#define PINOUT_FONT_BODY  getFont("UbuntuMono10")

struct PinoutGroup
{
    const char* name;
    const char* const* pins;
    uint8_t count;
};

struct PinoutCard
{
    const char* title;
    const char* subtitle;
    const PinoutGroup* groups;
    uint8_t groupCount;
};

static const char* const watchyBtnMain[] = {
    "MENU  GPIO26",
    "BACK  GPIO25",
    "DOWN  GPIO4",
    "UP    GPIO35/32",
};

static const char* const esp32Power[] = {"3V3", "GND x2", "VIN/5V", "EN"};
static const char* const esp32I2c[] = {"G21 SDA", "G22 SCL"};
static const char* const esp32Spi[] = {"G18 SCK", "G19 MISO", "G23 MOSI", "G5 CS"};
static const char* const esp32Uart[] = {"TX0", "RX0", "G16", "G17"};
static const char* const esp32Adc[] = {"VP", "VN", "G34", "G35", "G32", "G33"};
static const char* const esp32Gpio[] = {"G2", "G4", "G12", "G14", "G15", "G25", "G26", "G27"};

static const char* const c3Power[] = {"5V", "GND", "3V3", "RST"};
static const char* const c3I2c[] = {"G8 SDA", "G9 SCL"};
static const char* const c3Spi[] = {"G4 SCK", "G5 MISO", "G6 MOSI", "G7 SS"};
static const char* const c3Uart[] = {"G20 RX", "G21 TX"};
static const char* const c3Adc[] = {"G0 A0", "G1 A1", "G2 A2", "G3 A3", "G4 A4", "G5 A5"};
static const char* const c3Gpio[] = {"G0", "G1", "G2", "G3", "G4", "G5", "G6", "G7", "G8", "G9", "G10"};

static const char* const s3Power[] = {"5V", "GND", "3V3", "VBAT pads"};
static const char* const s3UsbSide[] = {"TX0", "RX0", "G1", "G2", "G3", "G4", "G5", "G6", "G7"};
static const char* const s3Right[] = {"G8", "G9", "G10", "G11", "G12", "G13"};
static const char* const s3Notes[] = {"G48 RGB LED", "G9 BOOT on some", "Clone pinouts vary"};

static const char* const i2cPins[] = {"GND", "VCC", "SDA", "SCL"};
static const char* const uartPins[] = {"GND", "VCC", "TX -> RX", "RX <- TX"};
static const char* const spiPins[] = {"GND", "VCC", "SCK", "MISO", "MOSI", "CS"};
static const char* const jstPins[] = {"1  + BAT", "2  - GND", "Check polarity"};

static const PinoutGroup watchyGroups[] = {
    {"BTN", watchyBtnMain, 4},
};

static const PinoutGroup esp32Groups[] = {
    {"PWR", esp32Power, 4},
    {"I2C", esp32I2c, 2},
    {"SPI", esp32Spi, 4},
    {"UART", esp32Uart, 4},
    {"ADC", esp32Adc, 6},
    {"GPIO", esp32Gpio, 8},
};

static const PinoutGroup c3Groups[] = {
    {"PWR", c3Power, 4},
    {"I2C", c3I2c, 2},
    {"SPI", c3Spi, 4},
    {"UART", c3Uart, 2},
    {"ADC", c3Adc, 6},
    {"GPIO", c3Gpio, 11},
};

static const PinoutGroup s3Groups[] = {
    {"PWR", s3Power, 4},
    {"USB L", s3UsbSide, 9},
    {"USB R", s3Right, 6},
    {"NOTE", s3Notes, 3},
};

static const PinoutGroup i2cGroups[] = {
    {"PIN", i2cPins, 4},
};

static const PinoutGroup uartGroups[] = {
    {"PIN", uartPins, 4},
};

static const PinoutGroup spiGroups[] = {
    {"PIN", spiPins, 6},
};

static const PinoutGroup jstGroups[] = {
    {"PIN", jstPins, 3},
};

static const PinoutCard g_cards[] = {
    {"WATCHY BTN", "Watchy 1/1.5/2", watchyGroups, 1},
    {"ESP32 DEV", "Common 30 pin", esp32Groups, 6},
    {"ESP32-C3 SM", "Ali clone typical", c3Groups, 6},
    {"ESP32-S3 SM", "Ali clone typical", s3Groups, 4},
    {"I2C 4 PIN", "Qwiic/Stemma style", i2cGroups, 1},
    {"UART 4 PIN", "Cross TX/RX", uartGroups, 1},
    {"SPI 6 PIN", "Sensor/display bus", spiGroups, 1},
    {"JST BAT", "Check polarity", jstGroups, 1},
};

static uint8_t g_card = 0;
static uint8_t g_group = 0;

static uint8_t cardCount()
{
    return sizeof(g_cards) / sizeof(g_cards[0]);
}

static void clampGroup()
{
    if (g_group >= g_cards[g_card].groupCount) {
        g_group = 0;
    }
}

static void drawGroupTabs(const PinoutCard& card)
{
    dis->setFont(PINOUT_FONT_TITLE);
    dis->setTextSize(1);

    int x = 6;
    for (uint8_t i = 0; i < card.groupCount && i < 6; i++) {
        int w = 30;
        if (i == g_group) {
            dis->fillRect(x, 162, w, 13, SCBlack);
            dis->setTextColor(SCWhite);
        } else {
            dis->drawRect(x, 162, w, 13, SCBlack);
            dis->setTextColor(SCBlack);
        }
        dis->setCursor(x + 3, 171);
        dis->print(card.groups[i].name);
        x += w + 2;
    }
    dis->setTextColor(SCBlack);
}

static void drawGroupPins(const PinoutGroup& group)
{
    dis->setFont(PINOUT_FONT_BODY);
    dis->setTextSize(1);

    for (uint8_t i = 0; i < group.count; i++) {
        int col = i / 6;
        int row = i % 6;
        int x = 8 + col * 96;
        int y = 72 + row * 16;
        if (col > 1) break;

        dis->drawCircle(x + 5, y - 3, 3, SCBlack);
        dis->setCursor(x + 14, y);
        dis->print(group.pins[i]);
    }
}

static void renderPinoutWallet()
{
    const PinoutCard& card = g_cards[g_card];
    const PinoutGroup& group = card.groups[g_group];

    dis->fillScreen(SCWhite);
    dis->setTextColor(SCBlack);
    dis->setFont(PINOUT_FONT_TITLE);
    dis->setTextSize(1);
    dis->setCursor(8, 14);
    dis->print(card.title);
    dis->setCursor(156, 14);
    dis->print(String(g_card + 1) + "/" + String(cardCount()));

    dis->setFont(PINOUT_FONT_BODY);
    dis->setCursor(8, 31);
    dis->print(card.subtitle);

    dis->setFont(PINOUT_FONT_TITLE);
    dis->setCursor(8, 55);
    dis->print(group.name);
    dis->setCursor(154, 55);
    dis->print(String(g_group + 1) + "/" + String(card.groupCount));
    dis->drawFastHLine(5, 60, 190, SCBlack);

    drawGroupPins(group);
    drawGroupTabs(card);

    dis->setFont(PINOUT_FONT_TITLE);
    dis->setCursor(8, 194);
    dis->print(PINOUT_WALLET_HINT);

    dUChange = true;
}

void initPinoutWallet()
{
    g_card = 0;
    g_group = 0;
    renderPinoutWallet();
    disUp(true);
}

void loopPinoutWallet()
{
    buttonState btn = useButton();
    resetSleepDelay(SLEEP_EVERY_MS);

    if (btn == Up) {
        g_card = (g_card + cardCount() - 1) % cardCount();
        clampGroup();
        renderPinoutWallet();
    } else if (btn == Down) {
        g_card = (g_card + 1) % cardCount();
        clampGroup();
        renderPinoutWallet();
    } else if (btn == Menu) {
        g_group = (g_group + 1) % g_cards[g_card].groupCount;
        renderPinoutWallet();
    }

    disUp(false);
}

void exitPinoutWallet()
{
}

#endif
