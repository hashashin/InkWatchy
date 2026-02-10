#include "presenceBeacon.h"

#if PRESENCE_BEACON

#include "services/presenceBeaconSvc.h"

static int g_lineUuid = -1;
static int g_lineMajMin = -1;
static int g_lineEnabled = -1;
static int g_linePeriod = -1;
static int g_lineBurst = -1;
static int g_lineTx = -1;

static const char* txIdxToStr(int idx)
{
    switch (idx) {
        case 0: return "-12dBm";
        case 1: return "-6dBm";
        case 2: return "0dBm";
        case 3: return "+3dBm";
        default: return "?";
    }
}

static String fmtSec(uint32_t ms)
{
    return String(ms / 1000UL) + "s";
}

static void uiRefresh()
{
    if (g_lineEnabled >= 0) {
        genpage_change((String("Enabled: ") + (presenceBeaconSvcGetEnabled() ? "YES" : "NO")).c_str(), g_lineEnabled);
    }
    if (g_linePeriod >= 0) {
        genpage_change((String("Period:  ") + fmtSec(presenceBeaconSvcGetPeriodMs())).c_str(), g_linePeriod);
    }
    if (g_lineBurst >= 0) {
        genpage_change((String("Burst:   ") + String(presenceBeaconSvcGetBurstMs()) + "ms").c_str(), g_lineBurst);
    }
    if (g_lineTx >= 0) {
        genpage_change((String("TX:      ") + txIdxToStr(presenceBeaconSvcGetTxPowerIndex())).c_str(), g_lineTx);
    }
}

static void btnToggle()
{
    presenceBeaconSvcSetEnabled(!presenceBeaconSvcGetEnabled());
    presenceBeaconSvcRequestRestart();
    vibrateMotor(VIBRATION_ACTION_TIME);
    uiRefresh();
}

static void btnPeriod()
{
    uint32_t ms = presenceBeaconSvcGetPeriodMs();
    uint32_t s = ms / 1000UL;
    if (s <= 15) ms = 30UL * 1000UL;
    else if (s <= 30) ms = 60UL * 1000UL;
    else if (s <= 60) ms = 120UL * 1000UL;
    else if (s <= 120) ms = 300UL * 1000UL;
    else ms = 15UL * 1000UL;

    presenceBeaconSvcSetPeriodMs(ms);
    vibrateMotor(VIBRATION_ACTION_TIME);
    uiRefresh();
}

static void btnBurst()
{
    uint32_t ms = presenceBeaconSvcGetBurstMs();
    if (ms <= 600) ms = 1000;
    else if (ms <= 1000) ms = 1500;
    else if (ms <= 1500) ms = 2000;
    else ms = 600;

    presenceBeaconSvcSetBurstMs(ms);
    vibrateMotor(VIBRATION_ACTION_TIME);
    uiRefresh();
}

static void btnPower()
{
    int idx = presenceBeaconSvcGetTxPowerIndex();
    idx = (idx + 1) % 4;
    presenceBeaconSvcSetTxPowerIndex(idx);
    vibrateMotor(VIBRATION_ACTION_TIME);
    uiRefresh();
}

void initPresenceBeaconCfg()
{
    presenceBeaconSvcInit();

    init_general_page(10);
    general_page_set_title("iBeacon presence");
    genpage_set_center();

    GeneralPageButton button[] = {
        {"Toggle", btnToggle},
        {"Period", btnPeriod},
        {"Burst",  btnBurst},
        {"Power",  btnPower},
    };
    general_page_set_buttons(button, 4);

    g_lineUuid   = genpage_add((String("UUID: ") + presenceBeaconSvcGetUuid()).c_str());
    g_lineMajMin = genpage_add((String("Major/Minor: ") + String(presenceBeaconSvcGetMajor()) + "/" + String(presenceBeaconSvcGetMinor())).c_str());
    g_lineEnabled = genpage_add("Enabled: ");
    g_linePeriod  = genpage_add("Period:  ");
    g_lineBurst   = genpage_add("Burst:   ");
    g_lineTx      = genpage_add("TX:      ");
    genpage_add("HA: away timeout ~ 3x period");
    genpage_add("Use low TX for battery");

    uiRefresh();
}

void loopPresenceBeaconCfg()
{
    general_page_set_main();
    slint_loop();
}

void exitPresenceBeaconCfg()
{
    slintExit();
}

#endif
