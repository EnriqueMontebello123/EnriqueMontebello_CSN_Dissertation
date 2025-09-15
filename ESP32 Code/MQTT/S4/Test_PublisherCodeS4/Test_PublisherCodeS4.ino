#include <WiFi.h>
#include <PubSubClient.h>
#include <AES.h>

// WiFi Setup
const char* ssid = "ARRIS-D2B9";
const char* password = "M0ntebe110!";

// MQTT Broker
const char* mqtt_server = "192.168.0.121";

// AES key
AES128 aes;
const byte aesKey[16] = {
  0x2b, 0x7e, 0x15, 0x16,
  0x28, 0xae, 0xd2, 0xa6,
  0xab, 0xf7, 0x6b, 0x00,
  0x45, 0x7e, 0x25, 0x4f
};

WiFiClient espClient;
PubSubClient client(espClient);

int messageCount = 0;
const int maxMessages = 200;

void setup() {
    Serial.begin(115200);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected");

    client.setServer(mqtt_server, 1883);
}

void loop() {
    if (!client.connected()) {
        client.connect("ESP32_Publisher");
    }

    if (messageCount < maxMessages) {
        messageCount++;

        // Start CPU cycle counter
        uint32_t startCycles = ESP.getCycleCount();

        // build message
        int temp = random(15, 36);
        char plainText[33]; // Max 32 chars + null terminator
        snprintf(plainText, sizeof(plainText), "Temperature: %d (message: %03d)", temp, messageCount);

        // Pad to 32 bytes for  x2 AES blocks
        char padded[33] = {0};
        strncpy(padded, plainText, 32); //ensure null terminator if < 32

        byte encrypted[32];
        aes.setKey(aesKey, sizeof(aesKey));
        for (int i = 0; i < 2; i++) {
            memcpy(encrypted + (i * 16), padded + (i * 16), 16);
            aes.encryptBlock(encrypted + (i * 16), encrypted + (i * 16));
        }

        client.publish("test/topic", encrypted, 32);

        // Print unencrypted message
        Serial.println("Sent → " + String(plainText));

        uint32_t endCycles = ESP.getCycleCount();
        uint32_t cpuCyclesUsed = endCycles - startCycles;
        uint32_t freeHeap = ESP.getFreeHeap();

        Serial.printf("CPU Cycles Used: %u\n", cpuCyclesUsed);
        Serial.printf("Free Heap (RAM): %u bytes\n", freeHeap);

        delay(2000);
    }
}
