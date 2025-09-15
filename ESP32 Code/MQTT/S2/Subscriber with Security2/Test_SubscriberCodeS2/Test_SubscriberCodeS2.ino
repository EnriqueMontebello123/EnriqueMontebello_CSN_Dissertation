#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

// WiFi credentials
const char* ssid = "ARRIS-D2B9";
const char* password = "M0ntebe110!";

// MQTT Broker info
const char* mqtt_server = "192.168.0.121";
const int mqtt_port = 8883;//use tls port
const char* mqtt_user = "UserName";
const char* mqtt_pass = "password123!";

// CA certificate (same as in publisher)
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

// Secure client
WiFiClientSecure net;
PubSubClient client(net);

void callback(char* topic, byte* payload, unsigned int length) {
    uint32_t startCycles = ESP.getCycleCount();

    // Extract message payload
    String message = "";
    for (int i = 0; i < length; i++) {
        message += (char)payload[i];
    }

    // Print received message
    Serial.printf("Message arrived [%s] %s\n", topic, message.c_str());

    // Extract message number using basic parsing
    int msgIndex = message.indexOf("(message: ");
    int msgNumber = -1;
    if (msgIndex != -1) {
        msgNumber = message.substring(msgIndex + 10, message.length() - 1).toInt();
    }

    uint32_t endCycles = ESP.getCycleCount();
    uint32_t cpuCyclesUsed = endCycles - startCycles;
    uint32_t freeHeap = ESP.getFreeHeap();

    Serial.printf("CPU Cycles Used: %u\n", cpuCyclesUsed);
    Serial.printf("Free Heap (RAM): %u bytes\n", freeHeap);
}
void setup_wifi() {
    Serial.print("Connecting to ");
    Serial.println(ssid);
    
    WiFi.begin(ssid, password);
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\nWiFi connected. IP address: ");
    Serial.println(WiFi.localIP());
}

void reconnect() {
    while (!client.connected()) {
        Serial.print("Attempting MQTT connection...");
        if (client.connect("ESP32_Subscriber", mqtt_user, mqtt_pass)) {
            Serial.println("connected");
            client.subscribe("test/topic");
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" retrying in 5 seconds...");
            delay(5000);
        }
    }
}

void setup() {
    Serial.begin(115200);
    setup_wifi();

    // Load CA cert for TLS
    net.setCACert(ca_cert);

    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);
}

void loop() {
    if (!client.connected()) {
        reconnect();
    }
    client.loop();
}
