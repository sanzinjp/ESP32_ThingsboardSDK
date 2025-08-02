#include "OLEDdisplay.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <Arduino_MQTT_Client.h>
#include <DHT.h>
#include <Espressif_Updater.h>
#include <Icons.h>
#include <MQTT.h>
#include <OTA_Firmware_Update.h>
#include <Server_Side_RPC.h>
#include <Shared_Attribute_Update.h>
#include <TBShareAttributesSubscribe.h>
#include <TBSubscribeRPC.h>
#include <ThingsBoard.h>
#include <WiFi.h>
#include <variable.h>
#include <wifiConnect.h>

// === CONFIGURATIONS ===
#define ENCRYPTED false

// === Global Constants ===
DHT dht(DHTPIN, DHTTYPE);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#if ENCRYPTED
WiFiClientSecure espClient;
#else
WiFiClient espClient;
#endif

Espressif_Updater<> updater;

Arduino_MQTT_Client mqttClient(espClient);
Shared_Attribute_Update<1U, MAX_ATTRIBUTES> shared_update;
Server_Side_RPC<MAX_RPC_SUBSCRIPTIONS, MAX_RPC_RESPONSE> rpc;
OTA_Firmware_Update<> ota;
const std::array<IAPI_Implementation *, 3U> apis = {&shared_update, &rpc, &ota};

ThingsBoard tb(mqttClient, MAX_MESSAGE_RECEIVE_SIZE, MAX_MESSAGE_SEND_SIZE,
               Default_Max_Stack_Size, apis);

Preferences preferences;

// === Global Variables ===
float temp = 0.0;
float hum = 0.0;
uint16_t blinkIntervalMs = 1000;
uint16_t dht22IntervalMs = 60000;
uint16_t displayUpdateInterval = 30000;
bool subscribed = false;
bool ledState = false;
unsigned long lastBlink = 0;
unsigned long lastDHT22Read = 0;
static unsigned long lastUpdate = 0;
bool currentFWSent = false;
bool updateRequestSent = false;

// === Function Prototypes ===
void initializeDisplay();
void updateCountdownTimers();
void handleBlink();
void handleTelemetry();
void handleDisplayUpdate();

// firmware update section
void update_starting_callback() { Serial.println("Update is starting..."); }

// Callback that will be called when the firmware update is finished
void finished_callback(const bool &success) {
  if (success) {
    Serial.println("Done, Reboot now");
#ifdef ESP8266
    ESP.restart();
#else
#ifdef ESP32
    esp_restart();
#endif
#endif
  } else {
    Serial.println("Downloading firmware failed");
  }
}

void progress_callback(const size_t &current, const size_t &total) {
  Serial.printf("Progress %.2f%%\n",
                static_cast<float>(current * 100U) / total);
}
// firmware update section

// === SETUP ===
void setup() {
  Serial.begin(SERIAL_DEBUG_BAUD);
  InitWiFi();
  pinMode(LED, OUTPUT);
  Wire.begin(OLED_SDA, OLED_SCL);
  dht.begin();

  initializeDisplay();
  setupMqtt();
  ensureMqttConnection();
  sendClientAttributes();

  preferences.begin("tb-config", true);
  uint16_t storedLEDInterval = preferences.getUInt("blink_intv", 0);
  uint16_t storedDHT22Interval = preferences.getUInt("dht22_intv", 0);
  preferences.end();

  if (storedLEDInterval != 0)
    blinkIntervalMs = storedLEDInterval;
  if (storedDHT22Interval != 0)
    dht22IntervalMs = storedDHT22Interval;

  Serial.printf("Using LED Blink Interval: %d\n", blinkIntervalMs);
  Serial.printf("Using DHT22 Interval: %d\n", dht22IntervalMs);
}

// === LOOP ===
void loop() {
  tb.loop(); // very first call
  if (!reconnect())
    return;

  if (!tb.connected()) {
    Serial.printf(CONNECTING_MSG, THINGSBOARD_SERVER, TOKEN);
    if (!tb.connect(THINGSBOARD_SERVER, TOKEN, THINGSBOARD_PORT)) {
      Serial.println("Failed to connect");
      return;
    }
  }

  // // firmware update section
  // if (!currentFWSent) {
  //   currentFWSent = ota.Firmware_Send_Info(CURRENT_FIRMWARE_TITLE,
  //                                          CURRENT_FIRMWARE_VERSION);
  // }

  // if (!updateRequestSent) {
  //   Serial.println("Firmware Update Subscription...");
  //   const OTA_Update_Callback callback(
  //       CURRENT_FIRMWARE_TITLE, CURRENT_FIRMWARE_VERSION, &updater,
  //       &finished_callback, &progress_callback, &update_starting_callback,
  //       FIRMWARE_FAILURE_RETRIES, FIRMWARE_PACKET_SIZE);
  //   updateRequestSent = ota.Subscribe_Firmware_Update(callback);
  // }
  // // firmware update section

  if (!subscribed) {
    Serial.println("Subscribing for shared attribute updates and RPC...");
    constexpr std::array<const char *, MAX_ATTRIBUTES>
        SUBSCRIBED_SHARED_ATTRIBUTES = {LED_BLINK_INTERVAL_KEY,
                                        DHT22_INTERVAL_KEY};

    Shared_Attribute_Callback<MAX_ATTRIBUTES> callback(
        &processSharedAttributeUpdate, SUBSCRIBED_SHARED_ATTRIBUTES);
    if (!shared_update.Shared_Attributes_Subscribe(callback)) {
      Serial.println("Failed to subscribe for shared attribute updates");
      return;
    }

    const RPC_Callback callbacks[] = {{RPC_SET_STATE, &processSetState}};

    if (!rpc.RPC_Subscribe(std::begin(callbacks), std::end(callbacks))) {
      Serial.println("Failed to subscribe to RPC");
      return;
    }

    Serial.println("RPC and Server Attributes Subscribe done!");
    subscribed = true;
  }

  tb.loop();
  handleBlink();
  updateCountdownTimers();
  handleTelemetry();
  handleDisplayUpdate();
}

// === Function Definitions ===

void initializeDisplay() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    while (true)
      ;
  }
  display.clearDisplay();
  display.display();
  delay(500);
}

void updateCountdownTimers() {
  unsigned long now = millis();
  unsigned long elapsed = now - lastUpdate;

  // Countdown for Display Update (D-)
  if (elapsed < displayUpdateInterval) {
    int countdownD = (displayUpdateInterval - elapsed) / 1000;
    display.fillRect(90, 0, 30, 8, SSD1306_BLACK);
    display.setCursor(90, 0);
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.print("D-");
    display.print(countdownD);
  }

  display.display();
}

void handleBlink() {
  if (millis() - lastBlink > blinkIntervalMs) {
    lastBlink = millis();
    ledState = !ledState;
    digitalWrite(LED, ledState);
  }
}

void handleTelemetry() {
  if (millis() - lastDHT22Read > dht22IntervalMs) {
    lastDHT22Read = millis();
    hum = dht.readHumidity();
    temp = dht.readTemperature();
    tb.sendTelemetryData("humidity", roundf(hum * 10) / 10.0);
    tb.sendTelemetryData("temperature", roundf(temp * 10) / 10.0);
  }
}

void handleDisplayUpdate() {
  if (millis() - lastUpdate >= displayUpdateInterval || lastUpdate == 0) {
    lastUpdate = millis();

    temp = dht.readTemperature();
    hum = dht.readHumidity();

    if (isnan(temp) || isnan(hum)) {
      Serial.println(F("Failed to read from DHT sensor!"));
      return;
    }

    displaySensorData(display, temp, hum);
  }
}
