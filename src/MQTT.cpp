#include "MQTT.h"
#include <PubSubClient.h>
#include <WiFi.h>
#include <variable.h>
#include <wifiConnect.h>

PubSubClient client(espClient);
void setupMqtt() { client.setServer(THINGSBOARD_SERVER, THINGSBOARD_PORT); }

void ensureMqttConnection() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32Client", TOKEN, "")) {
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

void sendClientAttributes() {
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
