#include <Arduino_MQTT_Client.h>
#include <Shared_Attribute_Update.h>
#include <TBShareAttributesSubscribe.h>
#include <ThingsBoard.h>
#include <WiFi.h>
#include <variable.h>

void processSharedAttributeUpdate(const JsonObjectConst &data) {
  preferences.begin("tb-config", false);

  for (auto it = data.begin(); it != data.end(); ++it) {
    String key = it->key().c_str();

    if (key == LED_BLINK_INTERVAL_KEY) {
      // Ensure value is an integer
      if (it->value().is<uint16_t>()) {
        uint16_t newInterval = it->value().as<uint16_t>();
        blinkIntervalMs = newInterval;
        preferences.putUInt("blink_intv", newInterval); // Save to preferences
        Serial.print("Updated and saved blink interval to: ");
        Serial.println(newInterval);
      } else {
        Serial.println("Invalid type for LED blink interval. Must be integer.");
      }
    } else if (key == DHT22_INTERVAL_KEY) {
      // Ensure value is an integer
      if (it->value().is<uint16_t>()) {
        uint16_t newInterval = it->value().as<uint16_t>();
        dht22IntervalMs = newInterval;
        preferences.putUInt("dht22_intv", newInterval); // Save to preferences
        Serial.print("Updated and saved DHT22 interval to: ");
        Serial.println(newInterval);
      } else {
        Serial.println("Invalid type for DHT22 interval. Must be integer.");
      }
    }
  }

  preferences.end();
}