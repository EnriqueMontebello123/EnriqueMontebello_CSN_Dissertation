#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "esp_system.h"

BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;
BLE2902* pBLE2902;

volatile bool deviceConnected = false;
volatile bool authenticated = false;// set true after correct password
volatile bool sentRun         = false;//send 200 messages once

static const char* correctPassword = "123456";// Demo password for GATT write based authenticatio

uint32_t randBetween(uint32_t a, uint32_t b) { return a + (esp_random() % (b - a + 1)); }

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* p) override {
    deviceConnected = true;
    Serial.println("Client connected");
  }
  void onDisconnect(BLEServer* p) override {// Reset auth/session state when client leaves and resume advertising
    deviceConnected = false;
    authenticated = false;
    sentRun = false;
    Serial.println("Client disconnected");
    BLEDevice::startAdvertising();
  }
};

class AuthCallbacks : public BLECharacteristicCallbacks {//password check on WRITE
  void onWrite(BLECharacteristic* ch) override {
    std::string value = ch->getValue();
    Serial.print("Received: "); Serial.println(value.c_str());
    if (value == correctPassword) {
      authenticated = true;
      Serial.println("Authenticated!");
      ch->setValue("OK");
      ch->notify();//push "OK" via notification
    } else {
      authenticated = false;
      Serial.println("Authentication failed!");
      ch->setValue("FAIL");
      ch->notify();//push "FAIL" via notification
    }
  }
};

void setup() {
  Serial.begin(115200);
  BLEDevice::init("ESP32-Peripheral");

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  BLEService* pService = pServer->createService("180C");
  pCharacteristic = pService->createCharacteristic(
    "2A56",
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_WRITE |//central writes password here
    BLECharacteristic::PROPERTY_NOTIFY//notify auth result + data
  );
  pCharacteristic->setCallbacks(new AuthCallbacks());

  pBLE2902 = new BLE2902();
  pBLE2902->setNotifications(true);// allow notify from server side
  pCharacteristic->addDescriptor(pBLE2902);

  pCharacteristic->setValue("Waiting");
  pService->start();
  //Start advertising the service so centrals can find
  pServer->getAdvertising()->addServiceUUID("180C");
  pServer->getAdvertising()->start();
  Serial.println("Peripheral started, waiting for client...");
}

void loop() {
  static int counter = 1;

  if (deviceConnected && authenticated && !sentRun) {
    // Send exactly 200 messages
    for (counter = 1; counter <= 200; counter++) {
      char msg[64];
      uint32_t temp = randBetween(15, 36);
      snprintf(msg, sizeof(msg), "Temperature: %lu (message:%d)", (unsigned long)temp, counter);

      // Measure cycles and heap around the send
      uint32_t cycles_before = ESP.getCycleCount();

      pCharacteristic->setValue((uint8_t*)msg, strlen(msg));
      pCharacteristic->notify();

      uint32_t cycles_after = ESP.getCycleCount();

      Serial.print("Sent message: ");
      Serial.println(msg);
      Serial.print("CPU cycles used: ");
      Serial.println(cycles_after - cycles_before);
      Serial.print("Free heap: ");
      Serial.println(ESP.getFreeHeap());

      delay(2000); // small pacing so central can keep up
    }
    sentRun = true;
  }

  delay(10);
}
