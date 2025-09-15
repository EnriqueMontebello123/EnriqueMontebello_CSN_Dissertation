#include <WiFi.h>
#include <PubSubClient.h>

// WiFi Setup
const char* ssid = "ARRIS-D2B9";
const char* password = "M0ntebe110!";

// MQTT Broker IP (no TLS)
const char* mqtt_server = "192.168.0.121";

WiFiClient espClient;
PubSubClient client(espClient);

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

  client.setServer(mqtt_server, 1883);
  randomSeed(micros());
}

void loop() {
  if (!client.connected()) {
    Serial.print("MQTT connect … ");
    if (client.connect("ESP32_Publisher")) {
      Serial.println("OK");
    } else {
      Serial.printf("failed rc=%d\n", client.state());
      delay(2000);
      return;
    }
  }

  if (messageCount < maxMessages) {
    messageCount++;

    // Measure CPU cycles
    uint32_t startCycles = ESP.getCycleCount();

    int temp = random(15, 36);
    String payload = "Temperature: " + String(temp) +
                     " (message: " + String(messageCount) + ")";
    client.publish("test/topic", payload.c_str());//publish message
    Serial.println("Sent → " + payload);

    // Print CPU and RAM usage
    uint32_t endCycles = ESP.getCycleCount();
    uint32_t cyclesUsed = endCycles - startCycles;
    Serial.printf("CPU Cycles Used: %u\n", cyclesUsed);
    Serial.printf("Free Heap (RAM): %u bytes\n", ESP.getFreeHeap());

    delay(2000);
  }
}
