#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
#include <BLE2902.h>
#include "esp_system.h"
#include <AES.h>

#define SERVICE_UUID        "180C"
#define CHARACTERISTIC_UUID "2A56"
static const char* targetName      = "ESP32-Peripheral";
static const char* correctPassword = "123456";

// same key as the peripheral
const uint8_t aesKey[16] = {
  0x2b,0x7e,0x15,0x16, 0x28,0xae,0xd2,0xa6, 0xab,0xf7,0x6b,0x00, 0x45,0x7e,0x25,0x4f
};

BLEClient* pClient = nullptr;
BLERemoteCharacteristic* pRemoteCharacteristic = nullptr;
bool doConnect = false, connected = false;

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice d) override {
    // stop scanning as soon as peripheral is detected
    if (d.getName() == targetName) {
      Serial.println("Found target device!");
      BLEDevice::getScan()->stop();
      doConnect = true;
    }
  }
};

// decrypt each 32-byte packet and print resource cost
void notifyCallback(BLERemoteCharacteristic*, uint8_t* pData, size_t length, bool) {
  if (length != 32) { // if not 32 bytes (2 AES blocks), skip payload to avoic crashing
    Serial.println("Invalid payload size. Skipping decryption.");
    return;
  }
  uint32_t cycles_before = ESP.getCycleCount();
  uint8_t decrypted[33];// +1 for NUL
  AES128 aes; aes.setKey(aesKey, sizeof(aesKey));

  // decrypt two 16-byte blocks in place
  memcpy(decrypted, pData, 32);
  aes.decryptBlock(decrypted +  0, decrypted +  0);
  aes.decryptBlock(decrypted + 16, decrypted + 16);
  decrypted[32] = '\0';

  uint32_t cycles_after = ESP.getCycleCount();

  Serial.print("Notification received: ");
  Serial.println((char*)decrypted);
  Serial.print("CPU cycles used: ");
  Serial.println(cycles_after - cycles_before);
  Serial.print("Free heap: ");
  Serial.println(ESP.getFreeHeap());
}

void connectToPeripheral() {
  BLEScan* scan = BLEDevice::getScan();
  BLEScanResults results = scan->start(5);

  for (int i = 0; i < results.getCount(); i++) {
    BLEAdvertisedDevice d = results.getDevice(i);
    if (d.getName() == targetName) {
      Serial.println("Connecting to peripheral...");
      pClient = BLEDevice::createClient();

      if (!pClient->connect(&d)) { Serial.println("Connect failed"); return; }

      //requests a larger MTU right after connect so 32-byte payloads fit for full outputs in serial monitor
      pClient->setMTU(185);
      delay(150);
      Serial.print("Negotiated MTU: "); Serial.println(pClient->getMTU());

      BLERemoteService* s = pClient->getService(SERVICE_UUID);
      if (!s) { Serial.println("Service not found"); return; }

      pRemoteCharacteristic = s->getCharacteristic(CHARACTERISTIC_UUID);
      if (!pRemoteCharacteristic) { Serial.println("Characteristic not found"); return; }

      // write the password once to unlock the run
      if (pRemoteCharacteristic->canWrite()) {
        pRemoteCharacteristic->writeValue((uint8_t*)correctPassword, strlen(correctPassword), false);
        Serial.println("Password sent!");
      }

      // subscribe to the encrypted notifications
      if (pRemoteCharacteristic->canNotify()) {
        pRemoteCharacteristic->registerForNotify(notifyCallback);
        Serial.println("Notification registered.");
      }

      connected = true;
      return;
    }
  }
  Serial.println("Peripheral not found.");
}

void setup() {
  Serial.begin(115200);
  BLEDevice::init("");

  // start scanning, callback flips doConnect when it sees the target name
  BLEScan* scan = BLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  scan->setActiveScan(true);
  scan->start(5);
}

void loop() {
  if (doConnect && !connected) { connectToPeripheral(); doConnect = false; }
  delay(200);
}
