#include <WiFi.h>
#include <PubSubClient.h>
#include <AES.h>

// WiFi Credentials
const char* ssid = "ARRIS-D2B9";
const char* password = "M0ntebe110!";

// MQTT Broker IP
const char* mqtt_server = "192.168.0.121";

// AES key same as the one on publisher
const byte aesKey[16] = {
  0x2b, 0x7e, 0x15, 0x16,
  0x28, 0xae, 0xd2, 0xa6,
  0xab, 0xf7, 0x6b, 0x00,
  0x45, 0x7e, 0x25, 0x4f
};
AES128 aes;

WiFiClient espClient;
PubSubClient client(espClient);

void callback(char* topic, byte* payload, unsigned int length) {
    uint32_t startCycles = ESP.getCycleCount();

    if (length != 32) {//check for payload length to avoid crashing incase payload is not 32bytes long
        Serial.println("Invalid payload size. Skipping decryption.");
        return;
    }

    byte decrypted[33]; // 32 bytes + null terminator
    aes.setKey(aesKey, sizeof(aesKey));

    for (int i = 0; i < 2; i++) {
        memcpy(decrypted + (i * 16), payload + (i * 16), 16);
        aes.decryptBlock(decrypted + (i * 16), decrypted + (i * 16));
    }
    decrypted[32] = '\0'; // Null-terminate the decrypted output

    Serial.printf("[%s] Encrypted message arrived. Decrypted message: %s\n", topic, decrypted);

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
        if (client.connect("ESP32_Subscriber")) {
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

    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);
}

void loop() {
    if (!client.connected()) {
        reconnect();
    }
    client.loop();
}
