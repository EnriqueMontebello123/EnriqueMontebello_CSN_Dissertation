#include <WiFiClientSecure.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <AES.h>

const char* ssid = "ARRIS-D2B9";
const char* password = "M0ntebe110!";
const char* mqtt_server = "192.168.0.121";
const int tls_port     = 8883;
const int non_tls_port = 1883;

const char* mqtt_user = "UserName";
const char* mqtt_pass = "password123!";

const char* ca_cert = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDBTCCAe2gAwIBAgIUTQ1yh8wavjPUVY/YsFYDbmITarswDQYJKoZIhvcNAQEL
BQAwEjEQMA4GA1UEAwwHTVFUVC1DQTAeFw0yNTA1MTgyMzE3MjdaFw0yNjA1MTgy
MzE3MjdaMBIxEDAOBgNVBAMMB01RVFQtQ0EwggEiMA0GCSqGSIb3DQEBAQUAA4IB
DwAwggEKAoIBAQC5hAwKX4+/OHOCxN7JBhvMfS/d6qeTLftTUjbzqpRwGzO4Qfps
MS3xLf4Zah80Kz8hGZqdBXgr+p6Eptstc9y2xz0Nj98AEtdWCjMXvy6XqYjTmfft
Xx/htgh/SKiNJjvRf+ajPje1Lwl7mAkxswtixWCez4rsJx2yyInO5S6eSqMUQQCo
WCbKghhV2N21a8rXSNtFXwEZ/dR+1mBA/HGV1kKJXxD4372hmxjGZjYyE7E53Mxn
uetC40p/veoZ0bPnC3Jk4qOFE3WKPe18UmWfgIGGtsSDoGc3rLfGG6lBM6+jlgoO
aTNHgmH8g0rHrDWEuGCSUZjT2SLx/guf4zSBAgMBAAGjUzBRMB0GA1UdDgQWBBTt
kzM5Af/jkqtxFYQqkuXKytNyFzAfBgNVHSMEGDAWgBTtkzM5Af/jkqtxFYQqkuXK
ytNyFzAPBgNVHRMBAf8EBTADAQH/MA0GCSqGSIb3DQEBCwUAA4IBAQB9Y3Cwtq5S
yrJiemmPEgcGYOUJtZbO1P+OE/G/2jVVpIejpDQeZgeFdTfawiapl8Z7rSq/yNj+
1dAzNYqA9yjG1wwDnbQsELTMu1DA/+ZHrGuwnfpdEhORP8q6hsH4Js7aFIa8Q1AP
waxGySGRJxexBZbGLms/hiNOtzAPGWgE7UvDHXsFG0qzix/cRu6/sCx+Jq++FKB2
Yjbh4LvNt82RT01MSy/M6uSaEpGOv/SPlTfE8pQpKGIfXvl+MGUNz3arTGnuqT0X
flXyfG4O2ySlkO4DHD7iw8gZT2jFgXkSfOZN3SUbx4QsNz8yUMaxFdiYhhDji07K
uST0nKHibJ70
-----END CERTIFICATE-----
)EOF";

WiFiClientSecure tlsClient;
WiFiClient       plainClient;
PubSubClient     clientTLS(tlsClient);      // TLS for auth
PubSubClient     clientPlain(plainClient);  // Plaintext for data

// === AES
AES128 aes;
const byte aesKey[16] = {
  0x2b,0x7e,0x15,0x16,0x28,0xae,0xd2,0xa6,
  0xab,0xf7,0x6b,0x00,0x45,0x7e,0x25,0x4f
};

const char* topic = "test/topic";

bool tlsConnected   = false;
bool plainConnected = false;

int messageCount = 0;
const int maxMessages = 200;

// Encrypt exactly 32 bytes (two 16-byte blocks), zero-padded if needed.
void encrypt32(const char* msg, byte out32[32]) {
  byte block[16];
  memset(out32, 0, 32);
  size_t len = strnlen(msg, 32);
  memcpy(out32, msg, len);
  aes.setKey(aesKey, sizeof(aesKey));

  // block 0
  memcpy(block, out32, 16);
  aes.encryptBlock(out32, block);
  // block 1
  memcpy(block, out32 + 16, 16);
  aes.encryptBlock(out32 + 16, block);
}

bool connectTLS() {
  clientTLS.setServer(mqtt_server, tls_port);
  tlsClient.setCACert(ca_cert);
  Serial.print("Connecting TLS...");
  bool ok = clientTLS.connect("ESP32_Pub_TLS", mqtt_user, mqtt_pass);
  Serial.println(ok ? "connected" : "failed");
  return ok;
}

bool connectPlain() {
  clientPlain.setServer(mqtt_server, non_tls_port);
  Serial.print("Connecting PLAIN...");
  bool ok = clientPlain.connect("ESP32_Pub_PLAIN");
  Serial.println(ok ? "connected" : "failed");
  return ok;
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nWiFi connected.");

  // 1) TLS auth
  tlsConnected = connectTLS();

  if (tlsConnected) {
    plainConnected = connectPlain();
    //Drop TLS once plaintext confirmed
    if (plainConnected) {
      delay(200);
      clientTLS.disconnect();
      tlsConnected = false;
      Serial.println("TLS disconnected; continuing on plaintext with AES-encrypted payloads.");
    }
  }
}

void loop() {
  if (tlsConnected)    clientTLS.loop();
  if (plainConnected)  clientPlain.loop();
  if (!plainConnected) plainConnected = connectPlain();

  if (plainConnected && messageCount < maxMessages) {
    messageCount++;
    uint32_t startCycles = ESP.getCycleCount();
    int temp = random(15, 36);
    char plain[32];  
    snprintf(plain, sizeof(plain), "Temperature:%d Msg:%03d", temp, messageCount);


    byte cipher[32];
    encrypt32(plain, cipher);

    // Publish
    clientPlain.publish(topic, cipher, sizeof(cipher));

    uint32_t endCycles = ESP.getCycleCount();

    Serial.print("Sent → "); Serial.println(plain);
    Serial.printf("CPU Cycles Used: %u\n", endCycles - startCycles);
    Serial.printf("Free Heap (RAM): %u bytes\n", ESP.getFreeHeap());

    delay(2000);
  }
}
