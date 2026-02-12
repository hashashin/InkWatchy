#include "presenceBeaconSvc.h"

#if PRESENCE_BEACON

// -----------------------------------------------------------------------------
// Config (override in config.h)
// -----------------------------------------------------------------------------
#ifndef PRESENCE_BEACON_UUID
#define PRESENCE_BEACON_UUID "2f234454-cf6d-4a0f-adf2-f4911ba9ffa6"
#endif

#ifndef PRESENCE_BEACON_MAJOR
#define PRESENCE_BEACON_MAJOR 1
#endif

#ifndef PRESENCE_BEACON_MINOR
#define PRESENCE_BEACON_MINOR 1
#endif

#ifndef PRESENCE_BEACON_PERIOD_MS
#define PRESENCE_BEACON_PERIOD_MS (30UL * 1000UL)
#endif

#ifndef PRESENCE_BEACON_BURST_MS
#define PRESENCE_BEACON_BURST_MS 1500UL
#endif

#ifndef PRESENCE_BEACON_MEASURED_PWR
#define PRESENCE_BEACON_MEASURED_PWR (-59)
#endif

#ifndef PRESENCE_BEACON_TX_PWR_INDEX
// 0:-12dBm, 1:-6dBm, 2:0dBm, 3:+3dBm
#define PRESENCE_BEACON_TX_PWR_INDEX 0
#endif

#ifndef PRESENCE_BEACON_DEFAULT_ENABLED
#define PRESENCE_BEACON_DEFAULT_ENABLED 0
#endif

// -----------------------------------------------------------------------------
// Internal state
// -----------------------------------------------------------------------------
static bool g_inited = false;
static bool g_enabled = (PRESENCE_BEACON_DEFAULT_ENABLED != 0);
static uint32_t g_periodMs = PRESENCE_BEACON_PERIOD_MS;
static uint32_t g_burstMs = PRESENCE_BEACON_BURST_MS;
static int g_txIndex = PRESENCE_BEACON_TX_PWR_INDEX;

static uint32_t g_lastKick = 0;
static bool g_advRunning = false;
static uint32_t g_advStopAt = 0;

static bool g_restartRequested = false;

static BLEAdvertising *g_adv = nullptr;

static uint32_t g_lastTickMs = 0;

static const esp_power_level_t kTxMap[] = {
    ESP_PWR_LVL_N12,
    ESP_PWR_LVL_N6,
    ESP_PWR_LVL_N0,
    ESP_PWR_LVL_P3,
};

static esp_power_level_t txFromIndex(int idx)
{
    if (idx < 0)
        idx = 0;
    int max = (int)(sizeof(kTxMap) / sizeof(kTxMap[0])) - 1;
    if (idx > max)
        idx = max;
    return kTxMap[idx];
}

static bool parseUuid16(const char *uuidStr, uint8_t out16[16])
{
    char buf[33] = {0};
    int bi = 0;

    for (const char *p = uuidStr; *p; ++p)
    {
        if (*p == '-')
            continue;
        if (!isxdigit((unsigned char)*p))
            return false;
        if (bi >= 32)
            return false;
        buf[bi++] = *p;
    }
    if (bi != 32)
        return false;

    auto hexNib = [](char c) -> int
    {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F')
            return 10 + (c - 'A');
        return -1;
    };

    for (int i = 0; i < 16; ++i)
    {
        int hi = hexNib(buf[i * 2]);
        int lo = hexNib(buf[i * 2 + 1]);
        if (hi < 0 || lo < 0)
            return false;
        out16[i] = (uint8_t)((hi << 4) | lo);
    }
    return true;
}

static BLEAdvertisementData makeIBeacon()
{
    uint8_t uuid16[16];
    if (!parseUuid16(PRESENCE_BEACON_UUID, uuid16))
    {
        memset(uuid16, 0, sizeof(uuid16));
    }

    uint8_t payload[25];
    payload[0] = 0x4C; // Apple
    payload[1] = 0x00;
    payload[2] = 0x02; // iBeacon type
    payload[3] = 0x15; // length
    memcpy(payload + 4, uuid16, 16);

    payload[20] = (uint8_t)((PRESENCE_BEACON_MAJOR >> 8) & 0xFF);
    payload[21] = (uint8_t)(PRESENCE_BEACON_MAJOR & 0xFF);
    payload[22] = (uint8_t)((PRESENCE_BEACON_MINOR >> 8) & 0xFF);
    payload[23] = (uint8_t)(PRESENCE_BEACON_MINOR & 0xFF);
    payload[24] = (uint8_t)((int8_t)PRESENCE_BEACON_MEASURED_PWR);

    BLEAdvertisementData data;
    data.setFlags(0x06);

    // BLE lib expects Arduino String.
    // IMPORTANT: use String(char*, len) to keep binary zeros.
    String mfg((const char *)payload, sizeof(payload));
    data.setManufacturerData(mfg);

    return data;
}

static void bleStopAndDeinit()
{
    if (g_adv)
    {
        g_adv->stop();
    }
    g_adv = nullptr;
    BLEDevice::deinit(false);
    g_advRunning = false;
}

static void bleStartBurst()
{
    BLEDevice::init("");

    esp_power_level_t pwr = txFromIndex(g_txIndex);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, pwr);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, pwr);

    g_adv = BLEDevice::getAdvertising();
    BLEAdvertisementData advData = makeIBeacon();
    g_adv->setAdvertisementData(advData);
    g_adv->setScanResponse(false);

    g_adv->setMinPreferred(0x06);
    g_adv->setMinPreferred(0x12);

    g_adv->start();
    g_advRunning = true;
    g_advStopAt = millis() + g_burstMs;
}

void presenceBeaconSvcInit()
{
    if (g_inited)
        return;
    g_inited = true;

    g_enabled = true;
    g_periodMs = PRESENCE_BEACON_PERIOD_MS;
    g_burstMs = PRESENCE_BEACON_BURST_MS;
    g_txIndex = PRESENCE_BEACON_TX_PWR_INDEX;

    g_lastKick = 0;
    g_advRunning = false;
    g_restartRequested = false;
}

void presenceBeaconSvcTick()
{
    if (!g_inited)
    {
        presenceBeaconSvcInit();
    }

    if (!g_enabled)
    {
        if (g_advRunning)
        {
            bleStopAndDeinit();
        }
        return;
    }

    uint32_t now = millis();
    // --- WiFi coexist guard: if WiFi is on/starting, don't touch BLE (prevents panic resets) ---
    wifi_mode_t wm = WiFi.getMode();
    if (wm != WIFI_OFF)
    {
        if (g_advRunning)
        {
            bleStopAndDeinit(); // stop beacon cleanly
        }
        return; // don't start new bursts while WiFi is enabled
    }
    // If we were "frozen" (night sleep / deep sleep), force a burst as soon as we wake.
    if (g_lastTickMs != 0)
    {
        uint32_t gap = now - g_lastTickMs;
        if (gap > 120000UL)
        {                              // > 2 minutes without ticks => we likely slept
            g_lastKick = 0;            // allow immediate burst
            g_restartRequested = true; // restart advertising cleanly
        }
    }
    g_lastTickMs = now;

    if (g_restartRequested)
    {
        if (g_advRunning)
        {
            bleStopAndDeinit();
        }
        g_restartRequested = false;
    }

    if (!g_advRunning)
    {
        if (g_lastKick == 0 || (now - g_lastKick) >= g_periodMs)
        {
            g_lastKick = now;
            bleStartBurst();
        }
    }
    else
    {
        if ((int32_t)(now - g_advStopAt) >= 0)
        {
            bleStopAndDeinit();
        }
    }
}

void presenceBeaconSvcStop()
{
    g_enabled = false;
    if (g_advRunning)
    {
        bleStopAndDeinit();
    }
}

bool presenceBeaconSvcGetEnabled() { return g_enabled; }
void presenceBeaconSvcSetEnabled(bool on)
{
    if (g_enabled == on)
        return;
    g_enabled = on;
    g_restartRequested = true;
}

uint32_t presenceBeaconSvcGetPeriodMs() { return g_periodMs; }
void presenceBeaconSvcSetPeriodMs(uint32_t ms)
{
    if (ms < 5000)
        ms = 5000;
    if (ms > 10UL * 60UL * 1000UL)
        ms = 10UL * 60UL * 1000UL;
    if (g_periodMs == ms)
        return;
    g_periodMs = ms;
}

uint32_t presenceBeaconSvcGetBurstMs() { return g_burstMs; }
void presenceBeaconSvcSetBurstMs(uint32_t ms)
{
    if (ms < 200)
        ms = 200;
    if (ms > 5000)
        ms = 5000;
    if (g_burstMs == ms)
        return;
    g_burstMs = ms;
    g_restartRequested = true;
}

int presenceBeaconSvcGetTxPowerIndex() { return g_txIndex; }
void presenceBeaconSvcSetTxPowerIndex(int idx)
{
    int max = (int)(sizeof(kTxMap) / sizeof(kTxMap[0])) - 1;
    if (idx < 0)
        idx = 0;
    if (idx > max)
        idx = max;
    if (g_txIndex == idx)
        return;
    g_txIndex = idx;
    g_restartRequested = true;
}

const char *presenceBeaconSvcGetUuid() { return PRESENCE_BEACON_UUID; }
uint16_t presenceBeaconSvcGetMajor() { return (uint16_t)PRESENCE_BEACON_MAJOR; }
uint16_t presenceBeaconSvcGetMinor() { return (uint16_t)PRESENCE_BEACON_MINOR; }

void presenceBeaconSvcRequestRestart()
{
    g_restartRequested = true;
}

#endif
