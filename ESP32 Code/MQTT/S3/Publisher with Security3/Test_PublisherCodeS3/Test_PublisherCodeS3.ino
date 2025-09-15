#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

// WiFi Setup
const char* ssid = "ARRIS-D2B9";
const char* password = "M0ntebe110!";

// MQTT Broker Setup
const char* mqtt_server = "192.168.0.121";
const int mqtt_port = 8883;
const char* mqtt_user = "UserName";
const char* mqtt_pass = "password123!";

// mTLS Crts/keys
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

const char* client_cert = R"KEY(
-----BEGIN CERTIFICATE-----
MIICsDCCAZgCFHyIE4juNX3AW3b7YCiPeA/sqkrpMA0GCSqGSIb3DQEBCwUAMBIx
EDAOBgNVBAMMB01RVFQtQ0EwHhcNMjUwNTIwMjIyOTM0WhcNMjYwNTIwMjIyOTM0
WjAXMRUwEwYDVQQDDAxQdWJsaXNoZXJFU1AwggEiMA0GCSqGSIb3DQEBAQUAA4IB
DwAwggEKAoIBAQCryvSeVI9+wyd8jV04vz3AfkEohkN1cRirSHiwRHdF5SDIypCN
7iPgO1DmRa3Wx0tHssd+wy/yTs8eOwYNlgUjrdrgjiYl0jB3JR9zfkJC+RvUnBzS
d+3vE4hgWai0aad3aU7SMRg8nKdhj7soBBEBI4x/iaxB/J1Xmjgl8qbHTt6Ut5GI
t2mFp00Wmdckvje8rBJzmp7Zx/4ZugN97z+axrZrBTNgfBEW7bNqvDM6C0YeA6OC
U93aWa1segwbfH19/0wtmHKdoD+GRSf9OxsMkGrIbeqYl6XzT7+BtONBKyYqXZcY
NtnCMQWLSi/0Buf95zzk5uDw2LnU7ei9ziknAgMBAAEwDQYJKoZIhvcNAQELBQAD
ggEBAAGqlWhOG/5G6fOls1CJsSB/iA/mPmuWaI/csoOv0Wvms+qG4m+stsk+myg0
CgTk9eaRWuyecKSqcvgcWcQXBWJu/IelFAPpxMTtWG9iGEBlGXWMCO8ra6LD4JSV
nn4WZxlm/8qJKLTLWgfiHs5uy45hgc8xUZ3h7DdygOlaZcH3pxcoIw4xexivFkl8
4cHgsIicLtLFtXRlmG0jcCYGqmZ+cw6hN1SAlI981l8LfBB/LdNlhgpZCZzoTsgg
uhS/AohqLwoL6iVOlpkOCATBcGskWZvRpxrZZJu3GiA9wTlp7YYVWnANSHRPLcl5
JjK1cw2Mr4FRBBW89QRubHBvv1U=
-----END CERTIFICATE-----
)KEY";

const char* client_key = R"KEY(
-----BEGIN PRIVATE KEY-----
MIIEvAIBADANBgkqhkiG9w0BAQEFAASCBKYwggSiAgEAAoIBAQCryvSeVI9+wyd8
jV04vz3AfkEohkN1cRirSHiwRHdF5SDIypCN7iPgO1DmRa3Wx0tHssd+wy/yTs8e
OwYNlgUjrdrgjiYl0jB3JR9zfkJC+RvUnBzSd+3vE4hgWai0aad3aU7SMRg8nKdh
j7soBBEBI4x/iaxB/J1Xmjgl8qbHTt6Ut5GIt2mFp00Wmdckvje8rBJzmp7Zx/4Z
ugN97z+axrZrBTNgfBEW7bNqvDM6C0YeA6OCU93aWa1segwbfH19/0wtmHKdoD+G
RSf9OxsMkGrIbeqYl6XzT7+BtONBKyYqXZcYNtnCMQWLSi/0Buf95zzk5uDw2LnU
7ei9ziknAgMBAAECggEABoXr2PE8C6OsZanWftegKCWDIt9KQ3Taa3KShprq7n//
STCL4Z/Hw/VsonnF2pBxttS+h30w3mIIskyqc6ydIFgzJfCv2SLMjVGfCpsiN/nO
iTmuTH2aTsgulv4Ajm0umzWn9QpBMdf+8RDsmd3hqWnopVUccdr+PHvSIvq+Q3YW
xakKu9Q4GLGpjdPg3plJH+NNVPdaHnxeTzTmspL7EYMHs9fUKYceIKcxTOycc80L
n3+uZduDBfJ5xcLcPvbkRz+uL18QorsDzcXnuNHvBp92GtrbwriAY7eXYu5+h1dY
wvfSi8IBK4p2LOOhPWaXLCku48SiiGTXAtW5T08BOQKBgQDLkBaGDHEgiuY9C38m
xSxBwUJykgb/PD2RydVe1Q+LAzxM1puDyBeuAM6Bard/qvcZBnyaNaP5oC6x5Bmv
E4wn/aPy9EKdivlEn1lOTU+Ti5pTDgyqxwYo6vfa1793ECpRMAVe7xDjzmxd4Nn7
Vq8M+Hyhlk+taluGALTUSD4prwKBgQDYC8xDNF77eTXMWpStIpoQm7+ARX/Cm+Vi
U16BokWJBqI0nPK2ZlDGEvOHyMjXhaqIfX6217rBxNdS81nUiUmnh0Dqu5k2q0z2
Zb4SgGBovSS45dj7WS3/qLmCXb3tNhu5A/RhJb+O0TBxnwoXMJJ8WhiPYPNylWs1
nH4yIoLuCQKBgAIkqDgNFXvUOoxV/Ka52Ldwg3SVWQGC7tsEFv0CnUFHbXEZqVV/
28/3LjVqnDf54tQen104R1uvdg5lamfPeuBI6Vr7e2ARQIKacuRCKB5Tj/Jj6Xo6
0jPIFYf2UYu9XdSw/r17IuBjIfzzCxh8Vyd5zkYdQQWAYypMA5tgj1b9AoGAH3So
SuinI6okQRq3JvYwxFZI7Z4w2d7k6QoGao173lWO7GYlmJUREaXUu2Jqi6a0i6bp
+iky0d+dEkDIRX+vr6qrswGZbzJFxGJP6FW0s8tGO32LuBzl66FfUTHg41bLdoay
4Cok1+BxUzm6uGGkEmLzzHxrNbW3pFiirx4DPjECgYBXunxwBu1t/SWnygh6E9/+
c3vJ5trP0b/pFivIOjRjE4t1Dpa25NWTU2azIa2jPwd7PKJ3PtK8jJJh43HpYZbP
oYZQEGwzOcZL4BM0pDnfrV9m92dleHx551t0oTf8OuFMdDdtPEVxN53bg3X3t9V2
nffoUF3DxLHYHX6d8ipw5w==
-----END PRIVATE KEY-----
)KEY";

// Secure client and MQTT client
WiFiClientSecure net;
PubSubClient mqtt(net);

uint32_t messageCount = 0;
const uint32_t maxMessages = 200;

void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  // mTLS setup
  net.setCACert(ca_cert);
  net.setCertificate(client_cert);
  net.setPrivateKey(client_key);

  mqtt.setServer(mqtt_server, mqtt_port);
}

void loop() {
  if (!mqtt.connected()) {
    Serial.print("MQTT connect … ");
    if (mqtt.connect("ESP32_Client", mqtt_user, mqtt_pass)) {
      Serial.println("OK");
    } else {
      Serial.printf("failed rc=%d\n", mqtt.state());
      delay(2000);
      return;
    }
  }

  if (messageCount < maxMessages) {
    messageCount++;

    // Start CPU counter
    uint32_t startCycles = ESP.getCycleCount();

    int temp = random(15, 36);
    String payload = "Temperature: " + String(temp) + 
                     " (message: " + String(messageCount) + ")";
    mqtt.publish("test/topic", payload.c_str());
    Serial.println("Sent → " + payload);

    uint32_t endCycles = ESP.getCycleCount();
    uint32_t cpuCyclesUsed = endCycles - startCycles;
    uint32_t freeHeap = ESP.getFreeHeap();

    Serial.printf("CPU Cycles Used: %u\n", cpuCyclesUsed);
    Serial.printf("Free Heap (RAM): %u bytes\n", freeHeap);

    delay(2000);
  }
}