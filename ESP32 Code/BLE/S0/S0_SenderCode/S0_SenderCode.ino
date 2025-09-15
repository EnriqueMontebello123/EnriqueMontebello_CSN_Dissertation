#include <NimBLEDevice.h>

NimBLECharacteristic* pCharacteristic;

uint32_t startCycle;
uint32_t endCycle;
uint32_t cyclesUsed;

void setup() {
  Serial.begin(115200);
  delay(5000);
  NimBLEDevice::init("ESP32-Peripheral");
  // Initialize with the name "ESP32-Peripheral"
  NimBLEServer* pServer = NimBLEDevice::createServer();//server to handle connections
  NimBLEService* pService = pServer->createService("12345678-1234-5678-1234-56789abcdef0"); // Create a BLE service
  pCharacteristic = pService->createCharacteristic( // Create a characteristic with READ and NOTIFY
    "abcdef01-1234-5678-1234-56789abcdef0",
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
  );
  pService->start();// Start the BLE service
 
  NimBLEAdvertisementData advData;
  advData.setName("ESP32-Peripheral");//advertised name
  advData.addServiceUUID("12345678-1234-5678-1234-56789abcdef0");// Advertised service

  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->setAdvertisementData(advData);
  NimBLEDevice::startAdvertising();

  Serial.println("Peripheral advertising...");
}

void loop() {
  //message loop
  for (int i = 1; i <= 200; i++) {
    startCycle = ESP.getCycleCount();

    int temp = random(15, 37);
    String msg = "Temperature : " + String(temp) + " (message: " + String(i) + ")";
    pCharacteristic->setValue(msg.c_str());
    pCharacteristic->notify();

    endCycle = ESP.getCycleCount();
    cyclesUsed = endCycle - startCycle;

    Serial.print("Sent: ");
    Serial.println(msg);
    Serial.print("Free heap: ");
    Serial.println(ESP.getFreeHeap());
    Serial.print("CPU cycles used: ");
    Serial.println(cyclesUsed);

    delay(2000);
  }

  while (true) {
    delay(1000);
  }
}
