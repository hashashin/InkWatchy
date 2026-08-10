#include "bleTagScanner.h"

#if BLE_TAG_SCANNER

#include "defines.h"

#define SCAN_INTERVAL_MS 10000

struct BleTagDevice
{
    const char *macAddress;
    const char *name;
};

#define BLE_TAG_DEVICE_ENTRY(mac, name) {mac, name},
static const BleTagDevice allowedDevices[] = {
    BLE_TAG_SCANNER_DEVICES(BLE_TAG_DEVICE_ENTRY)};
#undef BLE_TAG_DEVICE_ENTRY

static uint8_t parseBatteryPercentage(const uint8_t *payload, size_t length)
{
    if (payload == nullptr || length == 0)
    {
        return 255;
    }

    size_t n = 0;
    while (n < length)
    {
        uint8_t fieldLen = payload[n];
        if (fieldLen <= 0 || n + fieldLen >= length)
        {
            break;
        }

        uint8_t dataType = payload[n + 1];
        if (dataType == 0x21 && fieldLen >= 2)
        {
            uint8_t byteValue = payload[n + 2];
            float voltage = 1.800 + (byteValue * 0.006);
            float percentage = ((voltage - 1.8) / 1.5) * 100.0;
            return (uint8_t)percentage;
        }

        n += fieldLen + 1;
    }
    return 255;
}

static int findAllowedDeviceIndex(String macAddress)
{
    macAddress.toLowerCase();
    for (int i = 0; i < BLE_TAG_DEVICE_COUNT; i++)
    {
        String lowerAllowed = String(allowedDevices[i].macAddress);
        lowerAllowed.toLowerCase();
        if (macAddress == lowerAllowed)
        {
            return i;
        }
    }
    return -1;
}

struct ScannedDevice
{
    int deviceIndex;
    int rssi;
    uint8_t batteryPercentage;
};

static ScannedDevice scannedDevices[BLE_TAG_DEVICE_COUNT];
static uint16_t scannedDevicesLines[BLE_TAG_DEVICE_COUNT];
static int scannedDeviceCount = 0;
static bool pageNeedsUpdate = false;
static uint32_t lastScanTime = 0;

class BleTagExtScannerCallbacks : public BLEExtAdvertisingCallbacks
{
    void onResult(esp_ble_gap_ext_adv_report_t report) override
    {
        char addr_str[18];
        sprintf(addr_str, "%02x:%02x:%02x:%02x:%02x:%02x",
                report.addr[0], report.addr[1], report.addr[2],
                report.addr[3], report.addr[4], report.addr[5]);
        String macAddress = addr_str;
        debugLog("Found extended device: " + macAddress);

        int deviceIndex = findAllowedDeviceIndex(macAddress);
        if (deviceIndex < 0)
        {
            return;
        }

        debugLog("Found tag device");

        uint8_t battery = parseBatteryPercentage(
            report.adv_data,
            report.adv_data_len);

        for (int i = 0; i < scannedDeviceCount; i++)
        {
            if (scannedDevices[i].deviceIndex == deviceIndex)
            {
                scannedDevices[i].rssi = report.rssi;
                scannedDevices[i].batteryPercentage = battery;
                return;
            }
        }

        if (scannedDeviceCount < BLE_TAG_DEVICE_COUNT)
        {
            scannedDevices[scannedDeviceCount].deviceIndex = deviceIndex;
            scannedDevices[scannedDeviceCount].rssi = report.rssi;
            scannedDevices[scannedDeviceCount].batteryPercentage = battery;
            scannedDeviceCount++;
            pageNeedsUpdate = true;
        }
    }
};

static BleTagExtScannerCallbacks extScannerCallbacks;

static void onScanComplete(BLEScanResults results)
{
    debugLog("Tag scan finished");
}

void startBleScan()
{
    hostBleStartScanAsync(SCAN_INTERVAL_MS / 1000, onScanComplete);
}

void updateGeneralPage()
{
    if (!pageNeedsUpdate)
    {
        return;
    }
    pageNeedsUpdate = false;
    for (int i = 0; i < scannedDeviceCount; i++)
    {
        String line = allowedDevices[scannedDevices[i].deviceIndex].name;
        if (scannedDevices[i].batteryPercentage < 255)
        {
            line += " " + String(scannedDevices[i].batteryPercentage) + "%";
        }
        else
        {
            line += " --%";
        }
        line += "  " + String(scannedDevices[i].rssi);
        genpage_change(line.c_str(), scannedDevicesLines[i]);
    }
    general_page_set_main();
}

void initBleTagScanner()
{
    debugLog("Init BLE tag scanner called");

    hostBleInitClient();
    pBLEScan->setAdvertisedDeviceCallbacks(NULL);
    pBLEScan->setExtendedScanCallback(&extScannerCallbacks);
    pBLEScan->setExtScanParams();
    delay(100);

    startBleScan();
    lastScanTime = millis();

    resetSleepDelay(60 * 1000);

    init_general_page(BLE_TAG_DEVICE_COUNT);
    general_page_set_title("BLE Tag Scanning...");

    for(int i = 0; i < BLE_TAG_DEVICE_COUNT; i++) {
        scannedDevicesLines[i] = genpage_add("");
    }

    general_page_set_main();
}

void exitBleTagScanner()
{
    debugLog("Exit BLE tag scanner called");
    hostBleDeInitEverything();
}

void loopBleTagScanner()
{
    if (millis() - lastScanTime > SCAN_INTERVAL_MS)
    {
        lastScanTime = millis();
        startBleScan();
    }

    updateGeneralPage();
    slint_loop();
}

#endif
