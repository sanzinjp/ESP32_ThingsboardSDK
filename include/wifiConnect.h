#ifndef WIFI_CONNECT_H
#define WIFI_CONNECT_H

#include <WiFi.h>

extern WiFiClient espClient;
void InitWiFi();
bool reconnect();

#endif
