#include "subnetCalc.h"

#if SUBNET_CALC_APP

#include "rtcMem.h"
#include <stdint.h>
#include <stdio.h>

#define SUBNET_FONT_TITLE getFont("dogicapixel4")
#define SUBNET_FONT_MONO  getFont("UbuntuMono10")

#define SUBNET_VALUE_X 34

static uint8_t g_octets[4] = {192, 168, 1, 0};
static uint8_t g_cidr = 24;
static uint8_t g_field = 0;
static bool g_dirty = true;

static uint32_t ipToUint()
{
    return ((uint32_t)g_octets[0] << 24) |
           ((uint32_t)g_octets[1] << 16) |
           ((uint32_t)g_octets[2] << 8) |
           (uint32_t)g_octets[3];
}

static uint32_t maskFromCidr(uint8_t cidr)
{
    if (cidr == 0) return 0;
    return UINT32_MAX << (32 - cidr);
}

static String formatIp(uint32_t ip)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%lu.%lu.%lu.%lu",
             (unsigned long)((ip >> 24) & 0xff),
             (unsigned long)((ip >> 16) & 0xff),
             (unsigned long)((ip >> 8) & 0xff),
             (unsigned long)(ip & 0xff));
    return String(buf);
}

static String formatInputIp()
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%03u.%03u.%03u.%03u",
             g_octets[0], g_octets[1], g_octets[2], g_octets[3]);
    return String(buf);
}

static const char* fieldName()
{
    switch (g_field) {
        case 0: return "A";
        case 1: return "B";
        case 2: return "C";
        case 3: return "D";
        default: return "CIDR";
    }
}

static String usableHosts(uint8_t cidr)
{
    if (cidr == 0) return "4294967294";
    if (cidr == 32) return "1";
    if (cidr == 31) return "2";

    uint8_t hostBits = 32 - cidr;
    uint32_t total = UINT32_C(1) << hostBits;
    if (total <= 2) return "0";

    return String((unsigned long)(total - 2));
}

static void printResultLine(int y, const char* label, const String& value)
{
    dis->setCursor(8, y);
    dis->print(label);
    dis->setCursor(SUBNET_VALUE_X, y);
    dis->print(value);
}

static void renderSubnetCalc()
{
    uint32_t ip = ipToUint();
    uint32_t mask = maskFromCidr(g_cidr);
    uint32_t network = ip & mask;
    uint32_t broadcast = network | ~mask;

    uint32_t first = network;
    uint32_t last = broadcast;
    if (g_cidr < 31) {
        first = network + 1;
        last = broadcast - 1;
    }

    dis->fillScreen(SCWhite);
    dis->setTextColor(SCBlack);

    dis->setFont(SUBNET_FONT_TITLE);
    dis->setCursor(8, 14);
    dis->print(SUBNET_TITLE);

    dis->setFont(SUBNET_FONT_MONO);
    dis->setCursor(8, 34);
    dis->print(formatInputIp());

    char cidrBuf[8];
    snprintf(cidrBuf, sizeof(cidrBuf), "/%02u", g_cidr);
    dis->setCursor(158, 34);
    dis->print(cidrBuf);

    dis->drawFastHLine(5, 45, 190, SCBlack);

    printResultLine(62, SUBNET_LABEL_MASK, formatIp(mask));
    printResultLine(80, SUBNET_LABEL_NET, formatIp(network));
    printResultLine(98, SUBNET_LABEL_BCAST, formatIp(broadcast));
    printResultLine(116, SUBNET_LABEL_FIRST, formatIp(first));
    printResultLine(134, SUBNET_LABEL_LAST, formatIp(last));
    printResultLine(152, SUBNET_LABEL_HOSTS, usableHosts(g_cidr));

    dis->drawFastHLine(5, 165, 190, SCBlack);
    dis->setCursor(8, 184);
    dis->print(SUBNET_LABEL_EDIT);
    dis->print(fieldName());
    dis->setCursor(82, 184);
    dis->print(SUBNET_HINT);

    dUChange = true;
}

static uint8_t wrappedAdd(uint8_t value, int delta, uint8_t maxValue)
{
    int range = (int)maxValue + 1;
    int next = ((int)value + delta) % range;
    if (next < 0) next += range;
    return (uint8_t)next;
}

static void changeSelected(int delta)
{
    if (g_field < 4) {
        g_octets[g_field] = wrappedAdd(g_octets[g_field], delta, 255);
    } else {
        g_cidr = wrappedAdd(g_cidr, delta, 32);
    }
    g_dirty = true;
}

void initSubnetCalc()
{
    g_dirty = true;
    renderSubnetCalc();
    disUp(true);
}

void loopSubnetCalc()
{
    buttonState btn = useButton();

    resetSleepDelay(SLEEP_EVERY_MS);

    if (btn == Menu) {
        g_field = (g_field + 1) % 5;
        g_dirty = true;
    } else if (btn == Up) {
        changeSelected(1);
    } else if (btn == Down) {
        changeSelected(-1);
    } else if (btn == LongUp) {
        changeSelected(10);
    } else if (btn == LongDown) {
        changeSelected(-10);
    }

    if (g_dirty) {
        g_dirty = false;
        renderSubnetCalc();
    }

    disUp(false);
}

void exitSubnetCalc()
{
}

#endif
