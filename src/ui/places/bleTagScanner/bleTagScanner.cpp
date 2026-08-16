#include "bleTagScanner.h"

#if BLE_TAG_SCANNER

#include "defines.h"

// For some reason this helps, but maybe also be the reason in crowded spaced it can't detects stuff
#define SCAN_INTERVAL_MS 1000

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

void startBleScan()
{
    hostBleStartScanAsync(0, NULL);
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
    debugLog("updateGeneralPage end");
}

void initBleTagScanner()
{
    debugLog("Init BLE tag scanner called");
    scannedDeviceCount = 0;
    pageNeedsUpdate = false;

    hostBleInitClient();
    pBLEScan->setAdvertisedDeviceCallbacks(NULL);
    pBLEScan->setExtendedScanCallback(&extScannerCallbacks);

    // I assume whitelist + duplicate will fix problems in crowded places
    esp_ble_gap_clear_whitelist();
    for (int i = 0; i < BLE_TAG_DEVICE_COUNT; i++)
    {
        esp_bd_addr_t tag_addr;
        sscanf(allowedDevices[i].macAddress, "%02x:%02x:%02x:%02x:%02x:%02x",
               &tag_addr[0], &tag_addr[1], &tag_addr[2],
               &tag_addr[3], &tag_addr[4], &tag_addr[5]);
        esp_err_t ret = esp_ble_gap_update_whitelist(true, tag_addr, BLE_WL_ADDR_TYPE_RANDOM);
        if (ret == ESP_OK)
        {
            debugLog("Added to whitelist: " + String(allowedDevices[i].macAddress));
        }
        else
        {
            debugLog("Failed to add to whitelist: " + String(allowedDevices[i].macAddress) + " err:" + String(ret));
        }
    }

    esp_ble_ext_scan_params_t custom_params = {
        .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
        .filter_policy = BLE_SCAN_FILTER_ALLOW_ONLY_WLST,
        .scan_duplicate = BLE_SCAN_DUPLICATE_ENABLE,
        .cfg_mask = ESP_BLE_GAP_EXT_SCAN_CFG_UNCODE_MASK | ESP_BLE_GAP_EXT_SCAN_CFG_CODE_MASK,
        .uncoded_cfg = {BLE_SCAN_TYPE_PASSIVE, 4, 4},
        .coded_cfg = {BLE_SCAN_TYPE_PASSIVE, 4, 4},
    };
    esp_err_t rc = pBLEScan->setExtScanParams(&custom_params);
    if (rc != ESP_OK)
    {
        debugLog("Failed to set custom ext scan params: " + String(rc));
    }
    else
    {
        debugLog("Custom ext scan params set");
    }
    delay(100);

    startBleScan();
    lastScanTime = millis();

    resetSleepDelay(120000);

    init_general_page(BLE_TAG_DEVICE_COUNT);
    general_page_set_title("BLE Tag Scanning...");

    for (int i = 0; i < BLE_TAG_DEVICE_COUNT; i++)
    {
        scannedDevicesLines[i] = genpage_add("");
    }

    general_page_set_main();
}

void exitBleTagScanner()
{
    debugLog("Exit BLE tag scanner called");
    pBLEScan->stopExtScan();
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
