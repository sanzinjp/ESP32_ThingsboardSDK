#include "OLEDdisplay.h"
#include <Arduino_MQTT_Client.h>
#include <Server_Side_RPC.h>
#include <TBSubscribeRPC.h>
#include <ThingsBoard.h>
#include <WiFi.h>
#include <variable.h>

// === RPC callback implementation ===
void processSetState(const JsonVariantConst &data, JsonDocument &response) {
  Serial.println("Received RPC: setState");

  const char *command = data["command"];
  if (command) {
    Serial.print("Command: ");
    Serial.println(command);

    if (strcmp(command, "getCurrentData") == 0) {
      temp =
          roundf(dht.readTemperature() * 10) / 10.0; // get current temperature
      hum = roundf(dht.readHumidity() * 10) / 10.0;  // get current humidity

      tb.sendTelemetryData("temperature", temp); // send temperature
      tb.sendTelemetryData("humidity", hum);     // send humidity

      Serial.println("Telemetry sent : temperature " + String(temp) + ", humidity " +
                     String(hum));
      displaySensorData(display, temp, hum); // update display with current data
    }
  }
}