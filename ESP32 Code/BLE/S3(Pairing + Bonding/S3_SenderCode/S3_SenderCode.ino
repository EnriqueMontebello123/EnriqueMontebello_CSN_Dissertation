#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "esp_system.h"
#include <AES.h>
#include <cstring>
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"

BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;
BLE2902* pBLE2902 = nullptr;

volatile bool deviceConnected = false;
volatile bool secured         = false;
volatile bool sentRun         = false;// This code ensures we send 200 messages only once per session

// Fixed 128-bit key
const uint8_t aesKey[16] = {
  0x2b,0x7e,0x15,0x16, 0x28,0xae,0xd2,0xa6, 0xab,0xf7,0x6b,0x00, 0x45,0x7e,0x25,0x4f
};

uint32_t randBetween(uint32_t a, uint32_t b) { return a + (esp_random() % (b - a + 1)); }

// This code clears all BLE bonds stored if reset is enterd in serial
void clearAllBonds() {
  int n = esp_ble_get_bond_device_num();
  if (n <= 0) {
    Serial.println("No bonds to clear.");
    return;
  }
  esp_ble_bond_dev_t list[20];
  int count = n > 20 ? 20 : n;
  if (esp_ble_get_bond_device_list(&count, list) != ESP_OK) {
    Serial.println("Failed to get bond device list.");
    return;
  }
  int removed = 0;
  for (int i = 0; i < count; ++i) {
    if (esp_ble_remove_bond_device(list[i].bd_addr) == ESP_OK) removed++;
  }
  Serial.printf("Cleared %d bond(s).\n", removed);
}

// resets bonds and starts advertising if reset is entered via serial for testing
void checkSerialResetPeripheral() {
  if (!Serial.available()) return;
  String s = Serial.readStringUntil('\n');   // exact "reset" expected
  if (s == "reset") {
    Serial.println("Clearing all BLE bonds...");
    clearAllBonds();
    // Restart advertising immediately so pairing can start fresh.
    BLEAdvertising* adv = BLEDevice::getAdvertising();
    adv->stop();
    delay(100);
    adv->start();
    Serial.println("Advertising restarted. Ready to pair.");
  }
}

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer*) override {
    deviceConnected = true;
    secured = false;   // will change to true after authentication completes
    Serial.println("Client connected. Waiting for pairing/encryption...");
  }
  void onDisconnect(BLEServer*) override {
    deviceConnected = false;
    secured         = false;
    sentRun         = false;  // new session after disconnect
    Serial.println("Client disconnected. Restarting advertising.");
    BLEDevice::startAdvertising();
  }
};

// This code handles passkey display/confirm and marks when link is authenticated
class MySecurity : public BLESecurityCallbacks {
  void onPassKeyNotify(uint32_t pass_key) override {
    // This code displays the passkey to the user via Serial.
    Serial.printf("Display this passkey to user: %06u\n", pass_key);
  }
  bool onConfirmPIN(uint32_t pass_key) override {
    //confirm numeric comparison 
    Serial.printf("Confirm passkey: %06u\n", pass_key);
    return true;
  }
  bool onSecurityRequest() override {
    // This code accepts incoming security requests.
    return true;
  }
  uint32_t onPassKeyRequest() override {
    Serial.println("Central requested passkey input (ignored on peripheral).");
    return 0;
  }
  void onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) override {
    if (cmpl.success) {
      secured = true;// MITM + Bond + Link Encrypted (as configured below)
      Serial.println("Link authenticated + encrypted (MITM BOND).");
    } else {
      secured = false;
      Serial.printf("Authentication failed, reason=0x%02X\n", cmpl.fail_reason);
    }
  }
};

void setup() {
  Serial.begin(115200);
  delay(500);
  BLEDevice::init("ESP32-Peripheral");
  esp_err_t err = esp_ble_gap_config_local_privacy(true);
  // This code configures passkey + bonding; peripheral "displays" passkey (via Serial).
  BLESecurity *sec = new BLESecurity();
  sec->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND);
  sec->setCapability(ESP_IO_CAP_OUT); //display" passkey
  sec->setKeySize(16);
  sec->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  BLEDevice::setSecurityCallbacks(new MySecurity());

  // This code creates the server, service, and characteristic used for data exchange.
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  BLEService* svc = pServer->createService("180C");

  pCharacteristic = svc->createCharacteristic(
    "2A56",
    BLECharacteristic::PROPERTY_READ  |
    BLECharacteristic::PROPERTY_WRITE |
    BLECharacteristic::PROPERTY_NOTIFY
  );

  //This code requires encrypted+authenticated access at the attribute layer.
  pCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);

  pBLE2902 = new BLE2902();
  pBLE2902->setNotifications(false);
  //This ensuresonly an encrypted link can enable notifications.
  pBLE2902->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);
  pCharacteristic->addDescriptor(pBLE2902);

  pCharacteristic->setValue("Locked until encrypted");
  svc->start();

  //This code starts advertising
  BLEAdvertising *adv = BLEDevice::getAdvertising();
  adv->addServiceUUID("180C");
  adv->setScanResponse(true);
  adv->start();

  Serial.println("Peripheral ready. Pair to proceed.");
}

void loop() {
  //This code listens for "reset" from the serial monitor to clear bonds.
  checkSerialResetPeripheral();

  // This code sends 200 messages, one every 2s.
  if (deviceConnected && secured && pBLE2902->getNotifications() && !sentRun) {
    delay(50);
    AES128 aes;
    aes.setKey(aesKey, sizeof(aesKey));

    for (int msg = 1; msg <= 200; msg++) {
      int temp = randBetween(15, 36);
      char plainText[33];
      snprintf(plainText, sizeof(plainText),
               "Temperature: %d (message: %03d)", temp, msg);

      // This code pads to 32 bytes for 2 AES blocks.
      uint8_t padded[32] = {0};
      strncpy((char*)padded, plainText, 32);
      uint32_t cycles_before = ESP.getCycleCount();

      //This code encrypts two 16byte blocks
      uint8_t encrypted[32];
      memcpy(encrypted, padded, 32);
      aes.encryptBlock(encrypted +  0, encrypted +  0);
      aes.encryptBlock(encrypted + 16, encrypted + 16);

      uint32_t cycles_after = ESP.getCycleCount();
      //This code sends exactly 32 bytes as the notification payload
      pCharacteristic->setValue(encrypted, 32);
      pCharacteristic->notify();

      //This code outputs the CPU usage and free heap to the serial output
      Serial.print("Sent message: ");    Serial.println(plainText);
      Serial.print("CPU cycles used: "); Serial.println(cycles_after - cycles_before);
      Serial.print("Free heap: ");       Serial.println(ESP.getFreeHeap());

      delay(2000);
    }
    sentRun = true;
  }

  delay(10);
}
