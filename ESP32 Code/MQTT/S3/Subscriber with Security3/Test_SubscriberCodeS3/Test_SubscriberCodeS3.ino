#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

// WiFi credentials
const char* ssid = "ARRIS-D2B9";
const char* password = "M0ntebe110!";

// MQTT Broker info
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
MIICsTCCAZkCFHZuweneEfslUgyTXT9vm9HjXsLgMA0GCSqGSIb3DQEBCwUAMBIx
EDAOBgNVBAMMB01RVFQtQ0EwHhcNMjUwNTIwMjIyOTQ1WhcNMjYwNTIwMjIyOTQ1
WjAYMRYwFAYDVQQDDA1TdWJzY3JpYmVyRVNQMIIBIjANBgkqhkiG9w0BAQEFAAOC
AQ8AMIIBCgKCAQEAs7hzxM6xP3ILCrdL390IkBDcvXINpCT0uZYkRrQUkcMJub4R
fpY89PDwW7ngz+hB9USIcttxw3cLziNRt6sOiTLBQ/jzv4XFi/sWzKlBs22xbgyP
ab28JOqbmpWQWL6iKXc17j56QUIjEwry2aqsmgQUxeTRKEPOecccSwEy5TdrPJn2
MsZnlc+wgQHL/gPjzhYSd72Od3xvAMwYIzCf4X5w0yWVbgEnBB+EUpb7nlpzI6PM
Ue7updMTUDEyVK+yGsuUgcQHz5Rhq+ZVi16+Brs8bjRbwMy5Axkf4X/rKqqFDqKX
aXPJ/rhp/tYwLgVxcG3lt8IgcbQWFu98Z5XTNwIDAQABMA0GCSqGSIb3DQEBCwUA
A4IBAQBK1LGrXJtB6wgzUrzSNdWi8YXZPnqLDUb88DlbA7/HQAoe/82cfhMZIHHQ
eN51wHhWsNtIC6n8uwaPCna8YwAMFHIUsAlO7T8Mm4gWEzj1XizwsozC3Hz492F6
aMNfKgq0uU9j7Ka1w5fp5UmT4LuRRFGQPNINH5ZkmoGTJI9Vh5b603WzI/UaDjY1
KZArXLFN4+f6RmlG5NhyzDZ5TRo2puQpI8HlFa95u7Lafj0lOQjUx2aJRuoIuBPe
8nOhyLqkwbhZUQEwCu0IzIMW3Vrr/UENJDPQHLOBoZNG6cYpoFQYn1pPwKx8In6p
/cgyFw7nxEQdZzyoFnD6n26T1VFW
-----END CERTIFICATE-----
)KEY";

const char* client_key = R"KEY(
-----BEGIN PRIVATE KEY-----
MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQCzuHPEzrE/cgsK
t0vf3QiQENy9cg2kJPS5liRGtBSRwwm5vhF+ljz08PBbueDP6EH1RIhy23HDdwvO
I1G3qw6JMsFD+PO/hcWL+xbMqUGzbbFuDI9pvbwk6pualZBYvqIpdzXuPnpBQiMT
CvLZqqyaBBTF5NEoQ855xxxLATLlN2s8mfYyxmeVz7CBAcv+A+POFhJ3vY53fG8A
zBgjMJ/hfnDTJZVuAScEH4RSlvueWnMjo8xR7u6l0xNQMTJUr7Iay5SBxAfPlGGr
5lWLXr4GuzxuNFvAzLkDGR/hf+sqqoUOopdpc8n+uGn+1jAuBXFwbeW3wiBxtBYW
73xnldM3AgMBAAECggEAILW5nGBiMkH3cIPb82qvXEk1Y+WWRgHT6NNC1vt/ouf0
pTHKzVda0NnpZUh3el+zYYiPrRfzpLfOFj6naN5hTudF9bEh7AHU6p1QkP9dTeLx
IC55C63UyINKvt/p3b0Ulqdz0WDdr3LyH00m1icSMxX7EemLh+CPe+qZY61cLguR
kNVz+/+95+p9Ttb1flGoaluhFjwQ7zIjVV3E/l4nL5+JUJtc4wjf2Ug32IvNJbh4
WCYGuLMYzaMXKHx0cHNmYYFLoAvzhoROz9TvA0V0AIsSxR2m1Ioq1BsQ4HtJUD27
1g/saWC1AqP88u+wpPZR5h1xUJUk/vDYhEjZt98MQQKBgQDvvMqysDhQys5XP9Ze
BA1RxqklxXnlhNKmqPEuPQ1j08rnRs8jlhi6ygzlYWPx0do0t21PWGaLytulS771
UvJdgXO5Wfz8roz7EZJ2Z7JMGP8acoEkkIQiRRSXwU6/iLA95fPIs+KWasDjf/Pc
UcZt6PS/WdhV75JAwTRvUJK9kwKBgQC/6WybN0WXkq4ioF6siFFs2zZsOiM6IihE
1YN4JZkSNQwc3GtyDKq4weItmxnZLQu0BZK2sTWYlKiwjxmJenIm/nrFqhrBPxRV
3AHTSbHIgcHmOog06NYUohOIvIS/j0h79cemgRb86BOxRXAShAQlbhOEf8VFhvxq
QfTIvTK6TQKBgGQO/ubPh+Gok7B0s2rv9AMykDx0jGjQI/9tOaRb1O/aYLBgrGmk
5tNnKzS9jjJKrPEdsaDRO5OS6lO2JpBLu15tfjhZJ67qx2qurdt/cjoyoJ6QNfhl
3NBU1sBz7QMh8LUU+cfF1IeFLNaG++fztYcAGM6YmCNd50Phn5nqHiTDAoGBAKiD
BLR+IgNn7rPE8Zy5zhWxnuYFXq1LFOTsPpafHPuZzNsCyO3KJZY52KqHCHlkTmRy
iFMW46Lo1f7CiH/pHpcS2Sb7qKaLBwYlOw0WJp+TIHlSaAtdb62Ka3CjaET0QYdn
VznjHBZSiggcmr4HYcSkFUtBDi1wy+9ZnuEtEH/5AoGBAIG4AeCJkDsnmp2BJhQX
2WWykBxGV9cWs+0t9A2FL8N4BLQINcvKvVLgAL45MHMUyCJSjBrzy+0IF29GZXhY
qtQRh9EmcL8tzFL4YPwwiOv2CSDxEwH7zAoSwMRWADkZxW349tdGKisxpTpZZk1b
MHsigvATBM1ZpUsMYL8WQPpS
-----END PRIVATE KEY-----
)KEY";

// Secure client
WiFiClientSecure net;
PubSubClient client(net);

void callback(char* topic, byte* payload, unsigned int length) {
    uint32_t startCycles = ESP.getCycleCount();

    // Convert payload to string
    String message = "";
    for (int i = 0; i < length; i++) {
        message += (char)payload[i];
    }

    // Print message
    Serial.printf("Message arrived [%s] %s\n", topic, message.c_str());
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
        if (client.connect("SubscriberESP", mqtt_user, mqtt_pass)) {
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

    // Load Certs and key for mTLS
    net.setCACert(ca_cert);
    net.setCertificate(client_cert);
    net.setPrivateKey(client_key);

    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);
}

void loop() {
    if (!client.connected()) {
        reconnect();
    }
    client.loop();
}
