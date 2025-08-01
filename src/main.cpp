#include "MQTT.h"
#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  connectToWiFi();
  setupMqtt();
}

void loop() {
  ensureMqttConnection();
  sendAttributes();
  delay(10000); // Send every 10 seconds
}
