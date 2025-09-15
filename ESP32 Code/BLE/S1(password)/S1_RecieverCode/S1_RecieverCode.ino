#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
#include <BLE2902.h>
#include "esp_system.h"

// Target
#define SERVICE_UUID        "180C"
#define CHARACTERISTIC_UUID "2A56"
static const char* targetName = "ESP32-Peripheral";//advertised name
static const char* correctPassword = "123456";//password

BLEClient* pClient = nullptr;
BLERemoteCharacteristic* pRemoteCharacteristic = nullptr;
bool doConnect = false, connected = false;

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice d) override {
    if (d.getName() == targetName) {
      Serial.println("Found target device!");
      BLEDevice::getScan()->stop();
      doConnect = true;
    }
  }
};

void notifyCallback(BLERemoteCharacteristic*,
                    uint8_t* pData, size_t length, bool) {
  uint32_t cycles_before = ESP.getCycleCount();
  std::string value((char*)pData, length);
  uint32_t cycles_after = ESP.getCycleCount();


  Serial.print("Notification received: ");
  Serial.println(value.c_str());
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
    if (d.getName() == targetName) { // Connect to the advertiser
      Serial.println("Connecting to peripheral...");
      pClient = BLEDevice::createClient();

      if (!pClient->connect(&d)) { Serial.println("Connect failed"); return; }

      // Request larger MTU AFTER connect
      pClient->setMTU(185);
      delay(150); // give the MTU exchange a moment
      Serial.print("Negotiated MTU: ");
      Serial.println(pClient->getMTU());

      BLERemoteService* s = pClient->getService(SERVICE_UUID);//service/characteristic discovering
      if (!s) { Serial.println("Service not found"); return; }

      pRemoteCharacteristic = s->getCharacteristic(CHARACTERISTIC_UUID);
      if (!pRemoteCharacteristic) { Serial.println("Characteristic not found"); return; }

      if (pRemoteCharacteristic->canWrite()) {// Write demo password to characteristic
        pRemoteCharacteristic->writeValue((uint8_t*)correctPassword, strlen(correctPassword), false);
        Serial.println("Password sent!");
      }
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
  BLEScan* scan = BLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  scan->setActiveScan(true);
  scan->start(5);
}

void loop() {
  if (doConnect && !connected) { connectToPeripheral(); doConnect = false; }
  delay(200);
}
