#include <Adafruit_Sensor.h>
#include <Arduino_MQTT_Client.h>
#include <DHT.h>
#include <Shared_Attribute_Update.h>
#include <TBShareAttributesSubscribe.h>
#include <ThingsBoard.h>
#include <WiFi.h>
#include <variable.h>
#include <wifiConnect.h>

#define ENCRYPTED false

DHT dht(DHTPIN, DHTTYPE);

// Initialize underlying client, used to establish a connection
#if ENCRYPTED
WiFiClientSecure espClient;
#else
WiFiClient espClient;
#endif

uint16_t blinkIntervalMs = 1000;  // default value in milliseconds
uint16_t dht22IntervalMs = 60000; // default value in milliseconds

Preferences preferences;
//  Initalize the Mqtt client instance
Arduino_MQTT_Client mqttClient(espClient);
// Initialize used apis
Shared_Attribute_Update<1U, MAX_ATTRIBUTES> shared_update;
const std::array<IAPI_Implementation *, 1U> apis = {&shared_update};
// Initialize ThingsBoard instance with the maximum needed buffer size
ThingsBoard tb(mqttClient, MAX_MESSAGE_RECEIVE_SIZE, MAX_MESSAGE_SEND_SIZE,
               Default_Max_Stack_Size, apis);

// Statuses for subscribing to shared attributes
bool subscribed = false;

void setup() {
  pinMode(LED, OUTPUT);
  dht.begin();
  Serial.begin(SERIAL_DEBUG_BAUD);
  delay(1000);

  preferences.begin("tb-config", true); // read-only

  // Load blinkInterval only if it was previously saved (non-zero)
  uint16_t storedLEDInterval = preferences.getUInt("blink_intv", 0);
  Serial.print("storedLEDInterval: ");
  Serial.println(storedLEDInterval);

  if (storedLEDInterval != 0) {
    blinkIntervalMs = storedLEDInterval;
  }

  uint16_t storedDHT22Interval = preferences.getUInt("dht22_intv", 0);
  Serial.print("storedDHT22Interval: ");
  Serial.println(storedDHT22Interval);

  if (storedDHT22Interval != 0) {
    dht22IntervalMs = storedDHT22Interval;
  }

  preferences.end();
  Serial.print("Using LED Blink Interval: ");
  Serial.println(blinkIntervalMs);
  Serial.print("Using DHT22 Interval: ");
  Serial.println(dht22IntervalMs);

  InitWiFi();
}

unsigned long lastBlink, lastDHT22Read = 0;
bool ledState = false;

void loop() {

  if (!reconnect()) {
    return;
  }

  if (!tb.connected()) {
    // Reconnect to the ThingsBoard server,
    // if a connection was disrupted or has not yet been established
    Serial.printf(CONNECTING_MSG, THINGSBOARD_SERVER, TOKEN);
    if (!tb.connect(THINGSBOARD_SERVER, TOKEN, THINGSBOARD_PORT)) {
      Serial.println("Failed to connect");
      return;
    }
  }

  if (!subscribed) {
    Serial.println("Subscribing for shared attribute updates...");
    // Shared attributes we want to request from the server
    constexpr std::array<const char *, MAX_ATTRIBUTES>
        SUBSCRIBED_SHARED_ATTRIBUTES = {LED_BLINK_INTERVAL_KEY,
                                        DHT22_INTERVAL_KEY};

    const Shared_Attribute_Callback<MAX_ATTRIBUTES> callback(
        &processSharedAttributeUpdate, SUBSCRIBED_SHARED_ATTRIBUTES);
    if (!shared_update.Shared_Attributes_Subscribe(callback)) {
      Serial.println("Failed to subscribe for shared attribute updates");
      return;
    }

    Serial.println("Subscribe done");
    subscribed = true;
  }

  tb.loop();
  if (millis() - lastBlink > blinkIntervalMs) {
    lastBlink = millis();
    ledState = !ledState;
    digitalWrite(LED, ledState);
  }
  if (millis() - lastDHT22Read > dht22IntervalMs) {
    lastDHT22Read = millis();
    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();
    tb.sendTelemetryData("humidity", humidity);
    tb.sendTelemetryData("temperature", temperature);
  }
}
