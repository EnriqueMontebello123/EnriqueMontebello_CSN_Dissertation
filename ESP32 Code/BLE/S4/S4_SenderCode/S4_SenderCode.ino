#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "esp_system.h"
#include <AES.h>
#include <SHA256.h>
#include <cstring>
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"

BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;
BLE2902* pBLE2902 = nullptr;

volatile bool deviceConnected = false;
volatile bool secured         = false;
volatile bool sentRun         = false;   //send 200 messages only once per session

// Random per connection PIN 
char        sessionPin[7] = "000000";
volatile bool appAuthed       = false; // flips true after the nonce/HMAC is verified
uint8_t      lastNonce[16];                   // stores the current challenge
volatile bool challengeSent   = false;// ensures CHAL is sent only once per session

//timers
uint32_t tConnectMs   = 0;
uint32_t tSecuredMs   = 0;       
uint32_t tCCCDMs      = 0; 
uint32_t tChalMs      = 0;         

const uint32_t CONNECT_ENCRYPT_WINDOW_MS = 30000;// must encrypt within 30s of connecting
const uint32_t CCCD_WINDOW_MS            = 30000;// must enable notifications within 30s of encryption
const uint32_t AUTH_WINDOW_MS            = 15000;// must send valid RESP within 15s of CHAL

uint16_t lastConnId = 0; //to disconnect the current client 
volatile bool dropInProgress = false;  // prevents repeated disconnects until onDisconnect runs

// Fixed key
const uint8_t aesKey[16] = {
  0x2b,0x7e,0x15,0x16, 0x28,0xae,0xd2,0xa6, 0xab,0xf7,0x6b,0x00, 0x45,0x7e,0x25,0x4f
};

uint32_t randBetween(uint32_t a, uint32_t b) { return a + (esp_random() % (b - a + 1)); }

void startOpenAdvertising() {
  BLEAdvertising *adv = BLEDevice::getAdvertising();
  adv->stop();
  adv->addServiceUUID("180C");
  adv->setScanResponse(true);
  adv->start();
  Serial.println("Open advertising enabled (pairing allowed, public address).");
}

//clear all bonds if "reset" in Serial
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

void checkSerialResetPeripheral() {
  if (!Serial.available()) return;
  int c = Serial.peek();
  if (c != 'r' && c != 'R') return;
  String s = Serial.readStringUntil('\n'); s.trim();
  if (s == "reset" || s == "RESET") {
    Serial.println("Clearing all BLE bonds...");
    clearAllBonds();
    startOpenAdvertising();
    Serial.println("Advertising restarted (open). Ready to pair.");
  }
}

//per session pin
void makeNewSessionPin() {
  uint32_t x = esp_random() % 1000000;
  snprintf(sessionPin, sizeof(sessionPin), "%06u", x);
  Serial.println();
  Serial.println("========================================");
  Serial.printf("New SESSION PIN (demo): %s\n", sessionPin);
  Serial.println("========================================");
  Serial.flush();
}

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* s) override {
    deviceConnected = true;
    secured       = false;
    appAuthed     = false;
    challengeSent = false;
    sentRun       = false;

    tConnectMs = millis();
    tSecuredMs = 0;
    tCCCDMs    = 0;
    tChalMs    = 0;

    dropInProgress = false;
    lastConnId     = s->getConnId(); 

    makeNewSessionPin();              // new PIN every connection (bonded or not)
    Serial.println("Client connected. Waiting for pairing/encryption...");
  }
  void onDisconnect(BLEServer*) override {
    deviceConnected = false;
    secured       = false;
    appAuthed     = false;
    challengeSent = false;
    sentRun       = false;

    tConnectMs = tSecuredMs = tCCCDMs = tChalMs = 0;

    dropInProgress = false;           // ready for next client
    lastConnId     = 0;

    Serial.println("Client disconnected. Restarting advertising.");
    startOpenAdvertising();
  }
};

// Passkey display when link is authenticated.
class MySecurity : public BLESecurityCallbacks {
  void onPassKeyNotify(uint32_t pass_key) override {
    Serial.printf("Display this passkey to user: %06u\n", pass_key);
  }
  bool onConfirmPIN(uint32_t pass_key) override {
    Serial.printf("Confirm passkey: %06u\n", pass_key);
    return true;
  }
  bool onSecurityRequest() override { return true; }
  uint32_t onPassKeyRequest() override {
    Serial.println("Central requested passkey input (ignored on peripheral).");
    return 0;
  }
  void onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) override {
    if (cmpl.success) {
      secured   = true;
      tSecuredMs = millis();   // start CCCD timer
      Serial.println("Link authenticated.");
    } else {
      secured = false;
      Serial.printf("Authentication failed, reason=0x%02X\n", cmpl.fail_reason);
    }
  }
};

class AppCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* ch) override {
    std::string v = ch->getValue();

    if (v.size() == 5 && memcmp(v.data(), "HELLO", 5) == 0) {
      if (!challengeSent) {
        for (int i = 0; i < 16; i++) lastNonce[i] = (uint8_t)esp_random();
        uint8_t chal[5 + 16];
        memcpy(chal, "CHAL:", 5);
        memcpy(chal + 5, lastNonce, 16);
        pCharacteristic->setValue(chal, sizeof(chal));
        pCharacteristic->notify();
        challengeSent = true;
        tChalMs = millis();      // start RESP timer
        tCCCDMs = tCCCDMs ? tCCCDMs : millis(); 
        Serial.printf("HELLO received → Session CHAL sent. (PIN=%s)\n", sessionPin);
        Serial.flush();
      }
      return;
    }

    if (v.size() >= 5 && memcmp(v.data(), "RESP:", 5) == 0) {
      if (!challengeSent) { ch->setValue("FAIL"); ch->notify(); Serial.println("RESP before CHAL."); return; }
      if (v.size() < 5 + 16) { ch->setValue("FAIL"); ch->notify(); Serial.println("RESP too short."); return; }

      // Recompute chal and compare first 16 bytes.
      uint8_t mac[32];
      SHA256 h;
      h.resetHMAC((const uint8_t*)sessionPin, strlen(sessionPin));
      h.update(lastNonce, 16);
      h.finalizeHMAC((const uint8_t*)sessionPin, strlen(sessionPin), mac, sizeof(mac));

      if (memcmp(mac, v.data() + 5, 16) == 0) {
        appAuthed = true;
        tChalMs = 0; // stop the RESP timer
        ch->setValue("OK:"); ch->notify();
        Serial.println("Session auth OK (PIN+nonce verified).");
      } else {
        appAuthed = false;
        ch->setValue("FAIL"); ch->notify();
        Serial.println("Session auth FAIL (bad HMAC).");
        // Timer keeps running; if they stall timer will disconnect
      }
      return;
    }
  }
};

void setup() {
  Serial.begin(115200);
  delay(300); 
  Serial.println(); 
  Serial.flush();

// Init BLE 
  BLEDevice::init("ESP32-Peripheral");
  esp_err_t err = esp_ble_gap_config_local_privacy(false);

  //peripheral displa passkey via serial.
  BLESecurity *sec = new BLESecurity();
  sec->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND);
  sec->setCapability(ESP_IO_CAP_OUT); // we "display" passkey
  sec->setKeySize(16);
  sec->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  BLEDevice::setSecurityCallbacks(new MySecurity());

  // GATT server/service/characteristic
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  BLEService* svc = pServer->createService("180C");

  pCharacteristic = svc->createCharacteristic(
    "2A56",
    BLECharacteristic::PROPERTY_READ  |
    BLECharacteristic::PROPERTY_WRITE |
    BLECharacteristic::PROPERTY_NOTIFY
  );

  pCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);

  pBLE2902 = new BLE2902();
  pBLE2902->setNotifications(false);
  pBLE2902->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);
  pCharacteristic->addDescriptor(pBLE2902);

  pCharacteristic->setCallbacks(new AppCallbacks());
  pCharacteristic->setValue("Locked until encrypted");
  svc->start();
  startOpenAdvertising();
}

//force a disconnect (soft penalty so device can reconnectt)
void forceDisconnect(const char* reason) {
  if (dropInProgress) return;       
  dropInProgress = true;

  //Stop all timers so loop checks won't retrigger
  tConnectMs = tSecuredMs = tCCCDMs = tChalMs = 0;

  Serial.print(reason);
  Serial.println(" → disconnecting client (soft penalty).");

  if (pServer) {
    pServer->disconnect(lastConnId);
  }
}

void loop() {
  //"reset" in Serial to clear bonds
  checkSerialResetPeripheral();
  if (deviceConnected && !secured && !dropInProgress && tConnectMs &&
      (millis() - tConnectMs > CONNECT_ENCRYPT_WINDOW_MS)) {
    forceDisconnect("No encryption within window");
  }
  if (deviceConnected && secured && !dropInProgress) {
    if (tSecuredMs == 0) {
      tSecuredMs = millis();
    }
    if (!pBLE2902->getNotifications() && (millis() - tSecuredMs > CCCD_WINDOW_MS)) {
      forceDisconnect("No CCCD (notifications) within window after encryption");
    }
  }

  // Send the challenge once per session after security+subscription.
  if (deviceConnected && secured && pBLE2902->getNotifications() && !challengeSent) {
    delay(350);
    for (int i = 0; i < 16; i++) lastNonce[i] = (uint8_t)esp_random();
    uint8_t chal[5 + 16];
    memcpy(chal, "CHAL:", 5);
    memcpy(chal + 5, lastNonce, 16);
    pCharacteristic->setValue(chal, sizeof(chal));
    pCharacteristic->notify();
    challengeSent = true;
    tCCCDMs = millis();
    tChalMs = millis();   // start RESP timer
    Serial.printf("Session CHAL sent (nonce issued). (PIN=%s)\n", sessionPin);
    Serial.flush();
  }

  //must send valid RESP within time
  if (deviceConnected && challengeSent && !appAuthed && !dropInProgress && tChalMs &&
      (millis() - tChalMs > AUTH_WINDOW_MS)) {
    forceDisconnect("Auth window expired without valid RESP");
  }

  // Send 200 messages, one every 2s, only after session auth succeeds.
  if (deviceConnected && secured && pBLE2902->getNotifications() && appAuthed && !sentRun) {
    delay(50);
    AES128 aes; aes.setKey(aesKey, sizeof(aesKey));

    for (int msg = 1; msg <= 200; msg++) {
      int temp = randBetween(15, 36);
      char plainText[33];
      snprintf(plainText, sizeof(plainText), "Temperature: %d (message: %03d)", temp, msg);

      uint8_t padded[32] = {0};
      strncpy((char*)padded, plainText, 32);
      uint32_t cycles_before = ESP.getCycleCount();
      uint8_t encrypted[32];
      memcpy(encrypted, padded, 32);
      aes.encryptBlock(encrypted +  0, encrypted +  0);
      aes.encryptBlock(encrypted + 16, encrypted + 16);
      pCharacteristic->setValue(encrypted, 32);
      pCharacteristic->notify();
      uint32_t cycles_after = ESP.getCycleCount();
      Serial.print("Sent message: ");    Serial.println(plainText);
      Serial.print("CPU cycles used: "); Serial.println(cycles_after - cycles_before);
      Serial.print("Free heap: ");       Serial.println(ESP.getFreeHeap());

      delay(2000);
    }
    sentRun = true;
  }

  delay(10);
}
