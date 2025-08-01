#ifndef WIFI_MQTT_H
#define WIFI_MQTT_H

#include <Arduino.h>

void connectToWiFi();
void setupMqtt();
void ensureMqttConnection();
void sendAttributes();

#endif
