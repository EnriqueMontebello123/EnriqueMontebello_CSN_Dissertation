#include <WiFi.h>
#include <PubSubClient.h>

// WiFi Credentials
const char* ssid = "ARRIS-D2B9";
const char* password = "M0ntebe110!";

// MQTT Broker IP
const char* mqtt_server = "192.168.0.121";

// Creating WiFi and MQTT Client
WiFiClient espClient;
PubSubClient client(espClient);

// Callback when message is received
void callback(char* topic, byte* payload, unsigned int length) {
    uint32_t startCycles = ESP.getCycleCount();

    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    for (int i = 0; i < length; i++) {
        Serial.print((char)payload[i]);
    }
    Serial.println();

    // Measure CPU cycles and print heap usage
    uint32_t endCycles = ESP.getCycleCount();
    uint32_t cyclesUsed = endCycles - startCycles;
    Serial.printf("CPU Cycles Used: %u\n", cyclesUsed);
    Serial.printf("Free Heap (RAM): %u bytes\n", ESP.getFreeHeap());
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
        if (client.connect("ESP32_Subscriber")) {
            Serial.println("connected");
            client.subscribe("test/topic"); // Subscribe to a topic
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

    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);
}

void loop() {
    if (!client.connected()) {
        reconnect();
    }
    client.loop();
}
