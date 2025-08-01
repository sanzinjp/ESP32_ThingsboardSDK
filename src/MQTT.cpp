#include "MQTT.h"
#include <PubSubClient.h>
#include <WiFi.h>

// WiFi credentials
const char *ssid = "TP-Link_60_502";
const char *password = "isgm1234";

// ThingsBoard
const char *mqtt_server = "thingsboard.cloud";
const int mqtt_port = 1883;
const char *access_token = "vyT28nuMgr7DLU5W0SkH";

WiFiClient espClient;
PubSubClient client(espClient);

void connectToWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
}

void setupMqtt() { client.setServer(mqtt_server, mqtt_port); }

void ensureMqttConnection() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32Client", access_token, "")) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" trying again in 5 seconds");
      delay(5000);
    }
  }
  client.loop();
}

void sendAttributes() {
  String ssid = WiFi.SSID();
  String ip = WiFi.localIP().toString();
  String board = "ESP32-S3";
  String firmware = "v1.0.0";
  int freeHeap = ESP.getFreeHeap(); // Remaining free memory in bytes
  String mac = WiFi.macAddress();   // MAC address
  int rssi = WiFi.RSSI();           // Received signal strength indicator

  String payload = "{";
  payload += "\"ssid\":\"" + ssid + "\",";
  payload += "\"ip\":\"" + ip + "\",";
  payload += "\"board\":\"" + board + "\",";
  payload += "\"firmware\":\"" + firmware + "\",";
  payload += "\"free_heap\":" + String(freeHeap) + ",";
  payload += "\"mac\":\"" + mac + "\",";
  payload += "\"rssi\":" + String(rssi);
  payload += "}";

  bool sent = client.publish("v1/devices/me/attributes", payload.c_str());

  if (sent) {
    Serial.println("Attribute payload sent!");
    Serial.println(payload);
  } else {
    Serial.println("Failed to send attribute payload");
  }
}
