#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
#include <BLE2902.h>
#include "esp_system.h"
#include <AES.h>
#include <SHA256.h>
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
volatile bool gPaired = false; //flips true after link is encrypted
volatile bool appAuthed = false; //flips true after "OK" from peripheral

// Session auth (CHAL/RESP) state
volatile bool awaitingPin = false;
uint8_t chalNonce[16];
String  pinBuffer;
uint32_t chalRecvMillis = 0;
uint32_t subscribedAt   = 0;

//bond resetting function
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

//check for reset input for reserting bonds
void checkSerialResetCentral() {
  if (!Serial.available()) return;
  int c = Serial.peek();
  if (c != 'r' && c != 'R') return;
  String s = Serial.readStringUntil('\n'); s.trim();
  if (s != "reset" && s != "RESET") return;

  Serial.println("Reset: stop conn/scan, clear bonds, restart scan.");
  if (pClient && pClient->isConnected()) {
    pClient->disconnect();
    delay(100);
  }
  BLEScan* scan = BLEDevice::getScan();
  scan->stop(); delay(100); scan->clearResults();

  if (gTarget) { delete gTarget; gTarget = nullptr; }
  doConnect = false; connected = false;
  gPaired = false; appAuthed = false;
  awaitingPin = false; pinBuffer = ""; subscribedAt = 0;

  clearAllBonds();
  scan->start(0);
  Serial.println("Scanning...");
}

//Scan callbacks
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice d) override {
    bool nameMatch = d.haveName() && (d.getName() == targetName);
    bool svcMatch  = d.haveServiceUUID() && d.isAdvertisingService(BLEUUID(SERVICE_UUID));
    if (nameMatch || svcMatch) {
      Serial.printf("Found target: %s  RSSI:%d\n",
                    d.haveName() ? d.getName().c_str() : d.getAddress().toString().c_str(),
                    d.getRSSI());
      BLEDevice::getScan()->stop();
      if (gTarget) { delete gTarget; gTarget = nullptr; }
      gTarget = new BLEAdvertisedDevice(d);
      doConnect = true;
    }
  }
};

//pairing key
class MyClientSecurity : public BLESecurityCallbacks {
  uint32_t onPassKeyRequest() override {
    Serial.println("Enter 6-digit BLE pairing passkey:");
    String s;
    while (s.length() < 6) {
      if (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') break;
        if (isDigit(c)) { s += c; Serial.print('*'); }
      }
      delay(10);
    }
    Serial.println();
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

//notification handler
void notifyCallback(BLERemoteCharacteristic*, uint8_t* pData, size_t length, bool) {
  // Session password
  if (length >= 5 && memcmp(pData, "CHAL:", 5) == 0) {
    if (length < 5 + 16) { Serial.println("CHAL too short. Ignored."); return; }
    memcpy(chalNonce, pData + 5, 16);
    awaitingPin = true;
    appAuthed = false;
    chalRecvMillis = millis();
    pinBuffer = "";
    Serial.println("CHAL received. Enter session PIN from peripheral Serial and press Enter:");
    return;
  }

  // Session accepted
  if (length >= 3 && memcmp(pData, "OK:", 3) == 0) {
    appAuthed = true;
    awaitingPin = false;
    pinBuffer = "";
    Serial.println("Session auth OK (peripheral accepted our RESP).");
    return;
  }

  // Data payload 2 16byte AES blocks
  if (length != 32) {
    Serial.println("Invalid payload size. Skipping.");
    return;
  }
  uint32_t cycles_before = ESP.getCycleCount();
  uint8_t decrypted[33];
  AES128 aes; aes.setKey(aesKey, sizeof(aesKey));
  memcpy(decrypted, pData, 32);
  aes.decryptBlock(decrypted +  0, decrypted +  0);
  aes.decryptBlock(decrypted + 16, decrypted + 16);
  decrypted[32] = '\0';
  uint32_t cycles_after = ESP.getCycleCount();

  Serial.print("Notification: "); Serial.println((char*)decrypted);
  Serial.print("CPU cycles used: "); Serial.println(cycles_after - cycles_before);
  Serial.print("Free heap: ");       Serial.println(ESP.getFreeHeap());
}

// ---------- Connect & discover ----------
void cleanupAndRescan() {
  if (pClient) {
    if (pClient->isConnected()) pClient->disconnect();
  }
  if (gTarget) { delete gTarget; gTarget = nullptr; }
  BLEDevice::getScan()->start(0);
}

//connects, discovers, subscribes
void connectToPeripheral() {
  if (!gTarget) { Serial.println("No cached device to connect."); return; }

  Serial.println("Connecting to peripheral...");
  pClient = BLEDevice::createClient();

  if (!pClient->connect(gTarget)) {
    Serial.println("Connect failed");
    cleanupAndRescan();
    return;
  }

  //Request encryption(MITM
  gPaired = false;
  esp_err_t secReq = esp_ble_set_encryption(
      (uint8_t*)gTarget->getAddress().getNative(),
      ESP_BLE_SEC_ENCRYPT_MITM);
  Serial.printf("Security request status: 0x%02X\n", (int)secReq);

  uint32_t t0 = millis();
  while (!gPaired && (millis() - t0 < 30000)) delay(50);
  if (!gPaired) {
    Serial.println("Pairing timeout.");
    cleanupAndRescan();
    return;
  }

  // MTU & discovery
  pClient->setMTU(185);
  delay(120);
  Serial.print("Negotiated MTU: "); Serial.println(pClient->getMTU());

  BLERemoteService* s = pClient->getService(SERVICE_UUID);
  if (!s) {
    Serial.println("Service not found");
    cleanupAndRescan();
    return;
  }

  pRemoteCharacteristic = s->getCharacteristic(CHARACTERISTIC_UUID);
  if (!pRemoteCharacteristic) {
    Serial.println("Characteristic not found");
    cleanupAndRescan();
    return;
  }

  if (pRemoteCharacteristic->canNotify()) {
    pRemoteCharacteristic->registerForNotify(notifyCallback);
    Serial.println("Notification registered.");
  }

  connected     = true;
  appAuthed     = false;
  awaitingPin   = false;
  pinBuffer     = "";
  subscribedAt  = millis();
  delete gTarget; gTarget = nullptr;
}


void setup() {
  Serial.begin(115200);
  delay(500);

  BLEDevice::init("");
  // Keep central consistent with peripheral
  esp_err_t err = esp_ble_gap_config_local_privacy(false);
  BLESecurity *sec = new BLESecurity();
  sec->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND);
  sec->setCapability(ESP_IO_CAP_IN);   // we input the passkey
  sec->setKeySize(16);
  sec->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  BLEDevice::setSecurityCallbacks(new MyClientSecurity());

  BLEScan* scan = BLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  scan->setActiveScan(true);
  scan->setInterval(160);
  scan->setWindow(80);
  scan->start(0);
  Serial.println("Scanning...");
}

//session password entry
void handlePinEntryAndRespond() {
  if (!awaitingPin || !pRemoteCharacteristic) return;

  static uint32_t lastKeyMillis = 0;

  while (Serial.available()) {
    char c = Serial.read();
    if (c == 0x7F || c == 0x08) { // DEL or BS
      if (pinBuffer.length() > 0) {
        pinBuffer.remove(pinBuffer.length()-1);
        Serial.print("\b \b"); // erase last '*'
      }
      continue;
    }

    // Submit on Enter
    if (c == '\n' || c == '\r') {
      if (pinBuffer.length() >= 4) {
        Serial.println();
        goto SEND_RESP;
      } else {
        Serial.println("\nPIN too short, keep typing:");
        continue;
      }
    }

    if (isDigit(c) && pinBuffer.length() < 12) {
      pinBuffer += c;
      lastKeyMillis = millis();
      Serial.print('*');

      // Auto-submit at exactly 6 digits
      if (pinBuffer.length() == 6) {
        Serial.println();
        goto SEND_RESP;
      }
    }
  }

  //Timeouts: 30s overall
  if ((millis() - chalRecvMillis > 30000) ||
      (pinBuffer.length() > 0 && millis() - lastKeyMillis > 10000)) {
    Serial.println("\nPIN entry timeout. Reconnect or type 'reset'.");
    awaitingPin = false; pinBuffer = "";
  }
  return;

SEND_RESP: {
    //(PIN, nonce) → send first 16 bytes
    uint8_t mac[32];
    SHA256 h;
    h.resetHMAC((const uint8_t*)pinBuffer.c_str(), pinBuffer.length());
    h.update(chalNonce, 16);
    h.finalizeHMAC((const uint8_t*)pinBuffer.c_str(), pinBuffer.length(), mac, sizeof(mac));

    uint8_t resp[5 + 16];
    memcpy(resp, "RESP:", 5);
    memcpy(resp + 5, mac, 16);
    pRemoteCharacteristic->writeValue(resp, sizeof(resp), false);
    Serial.println("Session RESP written.");

    awaitingPin = false; pinBuffer = "";
  }
}

void loop() {
  checkSerialResetCentral();

  if (doConnect && !connected) {
    doConnect = false;
    connectToPeripheral();
  }

  handlePinEntryAndRespond();

  //If no CHAL arrives shortly after subscribe, request one
  if (connected && !appAuthed && !awaitingPin && subscribedAt &&
      (millis() - subscribedAt > 800) && pRemoteCharacteristic &&
      pRemoteCharacteristic->canWrite()) {
    const char* hello = "HELLO";
    pRemoteCharacteristic->writeValue((uint8_t*)hello, 5, false);
    Serial.println("HELLO written (requesting CHAL).");
    subscribedAt = 0;
  }

  //Handle disconnects
  if (pClient && connected && !pClient->isConnected()) {
    Serial.println("Disconnected from peripheral.");
    connected = false; appAuthed = false; awaitingPin = false; pinBuffer = ""; subscribedAt = 0;
    BLEDevice::getScan()->start(0);
  }

  delay(50);
}
