#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

// WiFi Setup
const char* ssid = "ARRIS-D2B9";
const char* password = "M0ntebe110!";

// MQTT Broker Setup
const char* mqtt_server = "192.168.0.121";
const int mqtt_port = 8883;//use tls port
const char* mqtt_user = "UserName";
const char* mqtt_pass = "password123!";

//ca.crt
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

// Use secure WiFi client
WiFiClientSecure net;
PubSubClient mqtt(net);

uint32_t messageCount = 0;
const uint32_t maxMessages = 200;

void setup() {
    Serial.begin(115200);

    // Connect to Wi-Fi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected");

    // Setup TLS certificate
    net.setCACert(ca_cert);

    // Connect to MQTT broker
    mqtt.setServer(mqtt_server, mqtt_port);
    randomSeed(micros());
}

void loop() {
    if (!mqtt.connected()) {
        Serial.print("MQTT connect … ");
        if (mqtt.connect("ESP32_Publisher", mqtt_user, mqtt_pass)) {
            Serial.println("OK");
        } else {
            Serial.printf("failed rc=%d\n", mqtt.state());
            delay(2000);
            return;
        }
    }

    if (messageCount < maxMessages) {
        messageCount++;

        // Start CPU cycle counter
        uint32_t startCycles = ESP.getCycleCount();

        // build and send message
        int temp = random(15, 36);
        String payload = "Temperature: " + String(temp) + 
                         " (message: " + String(messageCount) + ")";
        mqtt.publish("test/topic", payload.c_str());
        Serial.println("Sent → " + payload);

        // End cycle counter and RAM usage
        uint32_t endCycles = ESP.getCycleCount();
        uint32_t cpuCyclesUsed = endCycles - startCycles;
        uint32_t freeHeap = ESP.getFreeHeap();

        Serial.printf("CPU Cycles Used: %u\n", cpuCyclesUsed);
        Serial.printf("Free Heap (RAM): %u bytes\n", freeHeap);

        delay(2000);
    }
}
