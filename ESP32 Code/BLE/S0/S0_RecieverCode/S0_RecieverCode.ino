#include <ArduinoBLE.h>

// UUIDs matching peripheral ones
const char* serviceUUID        = "12345678-1234-5678-1234-56789abcdef0";
const char* characteristicUUID = "abcdef01-1234-5678-1234-56789abcdef0";

uint32_t startCycle;
uint32_t endCycle;
uint32_t cyclesUsed;

BLEDevice peripheral;
BLECharacteristic characteristic;

void setup() {
  Serial.begin(115200);
  while (!Serial);

  if (!BLE.begin()) {
    Serial.println("Starting BLE failed!");
    while (1);
  }

  Serial.println("Central: scanning for ESP32-Peripheral...");
  BLE.scan(); // start scanning
}

void loop() {
  // If not connected to a peripheral, keep scanning
  if (!peripheral) {
    BLEDevice dev = BLE.available();// Check if a device is found
    if (dev) {
      Serial.print("Found device: ");
      Serial.print(dev.address());
      Serial.print("  Name: ");
      Serial.println(dev.localName());// Print advertised name

      // Match by advertised name
      if (dev.localName() == "ESP32-Peripheral") {
        Serial.println("Target peripheral found, connecting...");
        BLE.stopScan();// Stop scanning once target is found
        if (dev.connect()) {
          Serial.println("Connected!");
          peripheral = dev;

          if (peripheral.discoverAttributes()) {// Discover services and characteristics
            Serial.println("Attributes discovered!");

            characteristic = peripheral.characteristic(characteristicUUID);
            if (characteristic) {
              Serial.println("Characteristic found!");
              if (characteristic.canSubscribe()) {// Subscribe to notifications if supported
                if (characteristic.subscribe()) {
                  Serial.println("Subscribed to notifications.");
                } else {
                  Serial.println("Subscribe failed!");// Reset and restart scanning
                  peripheral.disconnect();
                  peripheral = BLEDevice();
                  BLE.scan();
                }
              } else {
                Serial.println("Characteristic cannot subscribe.");
                peripheral.disconnect();
                peripheral = BLEDevice();
                BLE.scan();
              }
            } else {
              Serial.println("Characteristic not found.");
              peripheral.disconnect();
              peripheral = BLEDevice();
              BLE.scan();
            }
          } else {
            Serial.println("Attribute discovery failed!");
            peripheral.disconnect();
            peripheral = BLEDevice();
            BLE.scan();
          }
        } else {
          Serial.println("Failed to connect.");
        }
      }
    }
  } else {
    if (characteristic && characteristic.valueUpdated()) {// If connected, check for new notifications
  startCycle = ESP.getCycleCount();

  int valueLen = characteristic.valueLength();
  const uint8_t* rawData = characteristic.value();
  String value = "";
  for (int i = 0; i < valueLen; i++) {
    value += (char)rawData[i];
  }

  endCycle = ESP.getCycleCount();
  cyclesUsed = endCycle - startCycle;

  Serial.print("Received: ");
  Serial.println(value);
  Serial.print("Free heap: ");
  Serial.println(ESP.getFreeHeap());
  Serial.print("CPU cycles used: ");
  Serial.println(cyclesUsed);
}

    // Handle disconnection and restart scanning
    if (!peripheral.connected()) {
      Serial.println("Peripheral disconnected!");
      peripheral = BLEDevice();
      BLE.scan();
    }
  }
}
