#ifndef TB_SUBSCRIBE_RPC_H
#define TB_SUBSCRIBE_RPC_H

#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <ThingsBoard.h>

void processSetState(const JsonVariantConst &data, JsonDocument &response);
extern ThingsBoard tb;           // Global ThingsBoard instance
extern DHT dht;                  // DHT sensor instance
extern Adafruit_SSD1306 display; // OLED display instance

#endif // TB_SUBSCRIBE_RPC_H