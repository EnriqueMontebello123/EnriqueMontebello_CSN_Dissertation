#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "esp_system.h"
#include <AES.h>
#include <cstring>

BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;
BLE2902* pBLE2902 = nullptr;

volatile bool deviceConnected = false;
volatile bool authenticated   = false;
volatile bool sentRun         = false;  // to send the 200 msgs only once per session

//password 
static const char* correctPassword = "123456";

// fixed 128-bit key
const uint8_t aesKey[16] = {
  0x2b,0x7e,0x15,0x16, 0x28,0xae,0xd2,0xa6, 0xab,0xf7,0x6b,0x00, 0x45,0x7e,0x25,0x4f
};

uint32_t randBetween(uint32_t a, uint32_t b) { return a + (esp_random() % (b - a + 1)); }

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer*) override { deviceConnected = true; Serial.println("Client connected"); }
  void onDisconnect(BLEServer*) override {
    // new session after disconnect
    deviceConnected = false;
    authenticated   = false;
    sentRun         = false;
    Serial.println("Client disconnected");
    BLEDevice::startAdvertising();
  }
};

class AuthCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* ch) override {
    //any write here is to be considered as a password attempt
    std::string v = ch->getValue();
    Serial.print("Received: "); Serial.println(v.c_str());
    if (authenticated) { ch->setValue("OK"); ch->notify(); return; }

    if (v == correctPassword) {
      authenticated = true;
      Serial.println("Authenticated!");
      ch->setValue("OK"); ch->notify();
    } else {
      authenticated = false;
      Serial.println("Authentication failed!");
      ch->setValue("FAIL"); ch->notify();
    }
  }
};

void setup() {
  Serial.begin(115200);
  BLEDevice::init("ESP32-Peripheral");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  BLEService* svc = pServer->createService("180C");
  pCharacteristic = svc->createCharacteristic(
    "2A56",
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_WRITE |
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pCharacteristic->setCallbacks(new AuthCallbacks());

  pBLE2902 = new BLE2902();
  pBLE2902->setNotifications(true);
  pCharacteristic->addDescriptor(pBLE2902);

  pCharacteristic->setValue("Waiting");
  svc->start();
  pServer->getAdvertising()->addServiceUUID("180C");
  pServer->getAdvertising()->start();

  Serial.println("Peripheral ready");
}

void loop() {
  // start the run only when connected + authenticated, sends exactly 200 messages, every 2s
  if (deviceConnected && authenticated && !sentRun) {
    AES128 aes; 
    aes.setKey(aesKey, sizeof(aesKey));

    for (int msg = 1; msg <= 200; msg++) {
      // build plaintext; 32 chars so it fits 2 AES blocks
      int temp = randBetween(15, 36);
      char plainText[33];
      snprintf(plainText, sizeof(plainText),
               "Temperature: %d (message: %03d)", temp, msg);

      //pad to 32 bytes
      uint8_t padded[32] = {0};
      strncpy((char*)padded, plainText, 32);

      //resrouce cost measering
      uint32_t cycles_before = ESP.getCycleCount();

      // encrypt two 16-byte blocks in place
      uint8_t encrypted[32];
      memcpy(encrypted, padded, 32);
      aes.encryptBlock(encrypted +  0, encrypted +  0);
      aes.encryptBlock(encrypted + 16, encrypted + 16);

      uint32_t cycles_after = ESP.getCycleCount();

      // send exactly 32 bytes as the notification payload
      pCharacteristic->setValue(encrypted, 32);
      pCharacteristic->notify();

      //Serlial logging
      Serial.print("Sent message: ");    Serial.println(plainText);
      Serial.print("CPU cycles used: "); Serial.println(cycles_after - cycles_before);
      Serial.print("Free heap: ");       Serial.println(ESP.getFreeHeap());

      delay(2000);
    }
    sentRun = true; // wait for next session
  }

  delay(10);
}
