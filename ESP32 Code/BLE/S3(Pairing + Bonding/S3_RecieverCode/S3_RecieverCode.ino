#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
#include <BLE2902.h>
#include "esp_system.h"
#include <AES.h>
#include "esp_gap_ble_api.h"
#include "esp_bt_defs.h"

#define SERVICE_UUID        "180C"
#define CHARACTERISTIC_UUID "2A56"
static const char* targetName = "ESP32-Peripheral";

const uint8_t aesKey[16] = {
  0x2b,0x7e,0x15,0x16, 0x28,0xae,0xd2,0xa6, 0xab,0xf7,0x6b,0x00, 0x45,0x7e,0x25,0x4f
};

BLEClient* pClient = nullptr;
BLERemoteCharacteristic* pRemoteCharacteristic = nullptr;
BLEAdvertisedDevice* gTarget = nullptr;
bool doConnect = false, connected = false;
volatile bool gPaired = false;//flips true after authentication completes

// This code clears all BLE bonds stored
void clearAllBonds() {
  int n = esp_ble_get_bond_device_num();
  if (n <= 0) { Serial.println("No bonds to clear."); return; }
  esp_ble_bond_dev_t list[20];
  int count = n > 20 ? 20 : n;
  if (esp_ble_get_bond_device_list(&count, list) != ESP_OK) {
    Serial.println("Failed to get bond device list."); return;
  }
  int removed = 0;
  for (int i = 0; i < count; ++i)
    if (esp_ble_remove_bond_device(list[i].bd_addr) == ESP_OK) removed++;
  Serial.printf("Cleared %d bond(s).\n", removed);
}

// resets bonds and restarts scanning if reset is entered via serial for testing
void checkSerialResetCentral() {
  if (!Serial.available()) return;
  String s = Serial.readStringUntil('\n');   // exact "reset" expected
  if (s == "reset") {
    Serial.println("Clearing all BLE bonds...");
    clearAllBonds();

    //Restarts scanning so pairing can start fresh.
    BLEScan* scan = BLEDevice::getScan();
    scan->stop();
    delay(100);
    scan->start(0); // continuous
    Serial.println("Scanning restarted. Ready to re-pair.");
  }
}

//This code stops scanning when a match is seen
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice d) override {
    bool nameMatch = d.haveName() && (d.getName() == targetName);
    bool svcMatch  = d.haveServiceUUID() && d.isAdvertisingService(BLEUUID(SERVICE_UUID));
    if (nameMatch || svcMatch) {
      Serial.println("Found target device!");
      BLEDevice::getScan()->stop();
      if (gTarget) { delete gTarget; gTarget = nullptr; }
      gTarget = new BLEAdvertisedDevice(d); //This code copies the discovered device
      doConnect = true;
    }
  }
};

// This code asks for passkey and marks when link is authenticated.
class MyClientSecurity : public BLESecurityCallbacks {
  uint32_t onPassKeyRequest() override {
    //This code asks the user to enter the passkey in the Serial Monitor.
    Serial.println("Enter 6-digit passkey:");
    String s;
    while (s.length() < 6) {
      if (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') break;
        if (isDigit(c)) s += c;
      }
      delay(10);
    }
    uint32_t key = (uint32_t)s.toInt();
    Serial.printf("Using passkey: %06u\n", key);
    return key;
  }
  void onPassKeyNotify(uint32_t) override {}
  bool onConfirmPIN(uint32_t pass_key) override {
    Serial.printf("Confirm passkey: %06u\n", pass_key);
    return true;
  }
  bool onSecurityRequest() override { return true; }
  void onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) override {
    if (cmpl.success) {
      gPaired = true;
      Serial.println("Paired & Bonded. Link encrypted.");
    } else {
      gPaired = false;
      Serial.printf("Pairing failed, reason=0x%02X\n", cmpl.fail_reason);
    }
  }
};

// This code decrypts 32-byte payloads and prints CPU/heap usage.
void notifyCallback(BLERemoteCharacteristic*, uint8_t* pData, size_t length, bool) {
  if (length != 32) { Serial.println("Invalid payload size. Skipping decryption."); return; }

  //This code measures resource cost for decryption.
  uint32_t cycles_before = ESP.getCycleCount();
  uint8_t decrypted[33]; // +1 for NUL terminator
  AES128 aes; aes.setKey(aesKey, sizeof(aesKey));
  memcpy(decrypted, pData, 32);
  aes.decryptBlock(decrypted +  0, decrypted +  0);
  aes.decryptBlock(decrypted + 16, decrypted + 16);
  decrypted[32] = '\0';
  uint32_t cycles_after = ESP.getCycleCount();
  Serial.print("Notification received: ");
  Serial.println((char*)decrypted);
  Serial.print("CPU cycles used: ");
  Serial.println(cycles_after - cycles_before);
  Serial.print("Free heap: ");  Serial.println(ESP.getFreeHeap());
}

void connectToPeripheral() {
  if (!gTarget) { Serial.println("No cached device to connect."); return; }
  BLERemoteService* s = nullptr;

  Serial.println("Connecting to peripheral...");
  pClient = BLEDevice::createClient();

  if (!pClient->connect(gTarget)) {
    Serial.println("Connect failed");
    delete gTarget; gTarget = nullptr;
    // Restart scanning to try again
    BLEDevice::getScan()->start(0);
    return;
  }

  //Force pairing
  gPaired = false;
  esp_err_t secReq = esp_ble_set_encryption(*gTarget->getAddress().getNative(),
                                            ESP_BLE_SEC_ENCRYPT_MITM);
  Serial.printf("Security request status: 0x%02X\n", (int)secReq);

  uint32_t t0 = millis();
  while (!gPaired && (millis() - t0 < 30000)) {
    delay(50);
  }
  if (!gPaired) { Serial.println("Pairing timeout."); goto FAIL; }

  //This code requests a larger MTU so 32byte payloads are printed correctly
  pClient->setMTU(185);
  delay(150);
  Serial.print("Negotiated MTU: "); Serial.println(pClient->getMTU());

  s = pClient->getService(SERVICE_UUID);
  if (!s) { Serial.println("Service not found"); goto FAIL; }

  pRemoteCharacteristic = s->getCharacteristic(CHARACTERISTIC_UUID);
  if (!pRemoteCharacteristic) { Serial.println("Characteristic not found"); goto FAIL; }

  //This code subscribes to notifications
  if (pRemoteCharacteristic->canNotify()) {
    pRemoteCharacteristic->registerForNotify(notifyCallback);
    Serial.println("Notification registered.");
  }

  connected = true;
  delete gTarget; gTarget = nullptr;
  return;

FAIL:
  pClient->disconnect();
  delete gTarget; gTarget = nullptr;
  BLEDevice::getScan()->start(0);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  BLEDevice::init("");

  // This code configures passkey + bonding central inputs passkey via Serial.
  BLESecurity *sec = new BLESecurity();
  sec->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND);
  sec->setCapability(ESP_IO_CAP_IN);   // we input the passkey
  sec->setKeySize(16);
  sec->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  BLEDevice::setSecurityCallbacks(new MyClientSecurity());

  //This code starts continuous scanning
  BLEScan* scan = BLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  scan->setActiveScan(true);
  scan->setInterval(160);
  scan->setWindow(80);
  scan->start(0);
  Serial.println("Scanning...");
}

void loop() {
  // This code listens for "reset" from the serial monitor to clear bonds and restart scanning.
  checkSerialResetCentral();

  if (doConnect && !connected) {
    doConnect = false;
    connectToPeripheral();
  }

  delay(50);
}
