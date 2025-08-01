#if defined(ESP8266)
#include <ESP8266WiFi.h>
#define THINGSBOARD_ENABLE_PROGMEM 0
#elif defined(ESP32) || defined(RASPBERRYPI_PICO) || defined(RASPBERRYPI_PICO_W)
#include <WiFi.h>
#endif

#ifndef LED_BUILTIN
#define LED_BUILTIN 99
#endif

#include "DFRobot_PH.h"
#include "ORP_sensor.h"
#include "TdsSensor.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <Arduino_MQTT_Client.h>
#include <EEPROM.h>
#include <OneWire.h>
#include <ThingsBoard.h>


// Define Sensors Pins
#define DO_PIN A0
#define PH_PIN A1
const int DS18S20_PIN = D2;   // DS18S20 Signal pin on digital 2
const int orpPin = A3;        // ORP sensor Pin
const int TdsSensor_Pin = A2; // TDS(Water TDS Sensor)
const int buttonPin = D4;

#define pH_cali 0.0 // calibration value for pH
#define DO_cali 3.5 // calibration value for DO

// Define Ref values
#define VREF 3300      // VREF (mv)
#define ADC_RES 4096.0 // ADC Resolution

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Temperature chip i/o
OneWire ds(DS18S20_PIN); // on digital pin 2
float getTemp();

DFRobot_PH ph;

int intervalpH_eepromAddress = 11;
int intervalDS18B20_temp_eepromAddress = 12;
int intervalTDS_eepromAddress = 13;
int intervalORP_eepromAddress = 14;
int intervalDO_eepromAddress = 15;

// Single-point calibration Mode=0
// Two-point calibration Mode=1
#define TWO_POINT_CALIBRATION 0

#define READ_TEMP                                                              \
  (25) // Current water temperature ℃, Or temperature sensor function

// Single point calibration needs to be filled CAL1_V and CAL1_T
#define CAL1_V (1600) // mv
#define CAL1_T (25)   // ℃
// Two-point calibration needs to be filled CAL2_V and CAL2_T
// CAL1 High temperature point, CAL2 Low temperature point
#define CAL2_V (1300) // mv
#define CAL2_T (15)   // ℃

const uint16_t DO_Table[41] = {
    14460, 14220, 13820, 13440, 13090, 12740, 12420, 12110, 11810, 11530, 11260,
    11010, 10770, 10530, 10300, 10080, 9860,  9660,  9460,  9270,  9080,  8900,
    8730,  8570,  8410,  8250,  8110,  7960,  7820,  7690,  7560,  7430,  7300,
    7180,  7070,  6950,  6840,  6730,  6630,  6530,  6410};

uint8_t Temperaturet;
uint16_t ADC_Raw;
uint16_t ADC_Voltage;
uint16_t DO;

int16_t readDO(uint32_t voltage_mv, uint8_t temperature_c) {
#if TWO_POINT_CALIBRATION == 0
  uint16_t V_saturation =
      (uint32_t)CAL1_V + (uint32_t)35 * temperature_c - (uint32_t)CAL1_T * 35;
  return (voltage_mv * DO_Table[temperature_c] / V_saturation);
#else
  uint16_t V_saturation = (int16_t)((int8_t)temperature_c - CAL2_T) *
                              ((uint16_t)CAL1_V - CAL2_V) /
                              ((uint8_t)CAL1_T - CAL2_T) +
                          CAL2_V;
  return (voltage_mv * DO_Table[temperature_c] / V_saturation);
#endif
}

float voltage, phValue, temperature_pH = 25;

// Variables to store the button state
int buttonState = LOW;     // Current state of the button
int lastButtonState = LOW; // Previous state of the button

// pushButton Variables to store the timing
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

constexpr char WIFI_SSID[] = "i shrimp farming";
constexpr char WIFI_PASSWORD[] = "isgm1234";

// See https://thingsboard.io/docs/getting-started-guides/helloworld/
// to understand how to obtain an access token
// constexpr char TOKEN[] = "ycDeFncbQKS668iPgCPm"; DBrsJcHnNsCsfbV7FuBR ,
// 0cb8eP9hiS6rqfEt9Tn2
constexpr char TOKEN[] = "0cb8eP9hiS6rqfEt9Tn2";

// Thingsboard we want to establish a connection too
constexpr char THINGSBOARD_SERVER[] = "tb-iotpf.isgm.info";
// MQTT port used to communicate with the server, 1883 is the default
// unencrypted MQTT port.
constexpr uint16_t THINGSBOARD_PORT = 1883U;

// Maximum size packets will ever be sent or received by the underlying MQTT
// client, if the size is to small messages might not be sent or received
// messages will be discarded
constexpr uint32_t MAX_MESSAGE_SIZE = 1024U;

// Baud rate for the debugging serial connection.
// If the Serial output is mangled, ensure to change the monitor speed
// accordingly to this variable
constexpr uint32_t SERIAL_DEBUG_BAUD = 115200U;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Initialize underlying client, used to establish a connection
WiFiClient wifiClient;
// Initalize the Mqtt client instance
Arduino_MQTT_Client mqttClient(wifiClient);
// Initialize ThingsBoard instance with the maximum needed buffer size
ThingsBoard tb(mqttClient, MAX_MESSAGE_SIZE);

// Attribute names for attribute request and attribute updates functionality

constexpr char BLINKING_INTERVAL_ATTR[] = "blinkingInterval";
constexpr char LED_MODE_ATTR[] = "ledMode";
constexpr char LED_STATE_ATTR[] = "ledState";
constexpr char pHSensor_INTERVAL_ATTR[] = "pH_Sensor_Interval";
constexpr char DOSensor_INTERVAL_ATTR[] = "DO_Sensor_Interval";
constexpr char ORPSensor_INTERVAL_ATTR[] = "ORP_Sensor_Interval";
constexpr char TDSSensor_INTERVAL_ATTR[] = "TDS_Sensor_Interval";
constexpr char TEMPSensor_INTERVAL_ATTR[] = "TEMP_Sensor_Interval";

// handle led state and mode changes
volatile bool attributesChanged = false;

// LED modes: 0 - continious state, 1 - blinking
volatile int ledMode = 0;

// Current led state
volatile bool ledState = false;

// Settings for interval in blinking mode
constexpr uint16_t BLINKING_INTERVAL_MS_MIN = 10U;
constexpr uint16_t BLINKING_INTERVAL_MS_MAX = 60000U;
volatile uint16_t blinkingInterval = 1000U;
volatile uint16_t pH_Sensor_Interval = 1000U;
volatile uint16_t DO_Sensor_Interval = 1000U;
volatile uint16_t ORP_Sensor_Interval = 1000U;
volatile uint16_t TDS_Sensor_Interval = 1000U;
volatile uint16_t TEMP_Sensor_Interval = 1000U;

uint32_t previousStateChange;

// For telemetry
constexpr int16_t telemetrySendInterval = 2000U;
uint32_t previousDataSend;

// List of shared attributes for subscribing to their updates
constexpr std::array<const char *, 8U> SHARED_ATTRIBUTES_LIST = {
    LED_STATE_ATTR,          BLINKING_INTERVAL_ATTR,  pHSensor_INTERVAL_ATTR,
    DOSensor_INTERVAL_ATTR,  ORPSensor_INTERVAL_ATTR, TDSSensor_INTERVAL_ATTR,
    TEMPSensor_INTERVAL_ATTR};

// List of client attributes for requesting them (Using to initialize device
// states)
constexpr std::array<const char *, 1U> CLIENT_ATTRIBUTES_LIST = {LED_MODE_ATTR};

/// @brief Initalizes WiFi connection,
// will endlessly delay until a connection has been successfully established
void InitWiFi() {
  Serial.println("Connecting to AP ...");
  // Attempting to establish a connection to the given WiFi network
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    // Delay 500ms until a connection has been succesfully established
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected to AP");
}

/// @brief Reconnects the WiFi uses InitWiFi if the connection has been removed
/// @return Returns true as soon as a connection has been established again
const bool reconnect() {
  // Check to ensure we aren't connected yet
  const wl_status_t status = WiFi.status();
  if (status == WL_CONNECTED) {
    return true;
  }

  // If we aren't establish a new connection to the given WiFi network
  InitWiFi();
  return true;
}

/// @brief Processes function for RPC call "setLedMode"
/// RPC_Data is a JSON variant, that can be queried using operator[]
/// See https://arduinojson.org/v5/api/jsonvariant/subscript/ for more details
/// @param data Data containing the rpc data that was called and its current
/// value
/// @return Response that should be sent to the cloud. Useful for getMethods
RPC_Response processMeasureIt(const RPC_Data &data) {
  Serial.println("Measure the sensors and send to TB");
  delay(10);
  float temperature = getTemp();
  tb.sendTelemetryData("TEMP", temperature);
  Serial.println("temperature");
  Serial.println(temperature);

  delay(10);
  float tdsValue = getTdsValue(); // Get TDS sensor Data
  tb.sendTelemetryData("TDS", tdsValue);
  delay(10);

  voltage = analogRead(PH_PIN) / ADC_RES * VREF; // read the voltage
  // ph.begin();
  phValue = (ph.readPH(voltage, temperature_pH)) +
            pH_cali; // convert voltage to pH with temperature compensation
  tb.sendTelemetryData("pH", phValue);
  Serial.println("phValue");
  Serial.println(phValue);
  // Serial.println("voltage");
  // Serial.println(voltage);
  delay(10);
  double orp_sensor_Value = loopORP_sensor2();
  tb.sendTelemetryData("ORP", orp_sensor_Value);
  delay(10);
  Temperaturet = (uint8_t)READ_TEMP;
  ADC_Raw = analogRead(DO_PIN);
  ADC_Voltage = uint32_t(VREF) * ADC_Raw / ADC_RES;
  float DO_Value = ((readDO(ADC_Voltage, Temperaturet)) / 1000.0) + DO_cali;
  tb.sendTelemetryData("DO", DO_Value);

  // Process data
  int new_mode = data;

  Serial.print("Mode to change: ");
  Serial.println(new_mode);

  if (new_mode != 0 && new_mode != 1) {
    return RPC_Response("error", "Unknown mode!");
  }

  ledMode = new_mode;

  attributesChanged = true;

  // Returning current mode
  return RPC_Response("newMode", (int)ledMode);
}

// Optional, keep subscribed shared attributes empty instead,
// and the callback will be called for every shared attribute changed on the
// device, instead of only the one that were entered instead
const std::array<RPC_Callback, 1U> callbacks = {
    RPC_Callback{"measureIt", processMeasureIt}};

/// @brief Update callback that will be called as soon as one of the provided
/// shared attributes changes value, if none are provided we subscribe to any
/// shared attribute change instead
/// @param data Data containing the shared attributes that were changed and
/// their current value
void processSharedAttributes(const Shared_Attribute_Data &data) {
  for (auto it = data.begin(); it != data.end(); ++it) {
    if (strcmp(it->key().c_str(), BLINKING_INTERVAL_ATTR) == 0) {
      const uint16_t new_interval = it->value().as<uint16_t>();
      if (new_interval >= BLINKING_INTERVAL_MS_MIN &&
          new_interval <= BLINKING_INTERVAL_MS_MAX) {
        blinkingInterval = new_interval;
        Serial.print("Blinking interval is set to: ");
        Serial.println(new_interval);
      }
    } else if (strcmp(it->key().c_str(), LED_STATE_ATTR) == 0) {
      ledState = it->value().as<bool>();
      if (LED_BUILTIN != 99) {
        // digitalWrite(LED_BUILTIN, ledState);
      }
      Serial.print("LED state is set to: ");
      Serial.println(ledState);
    }
    // Get pH Interval from Dashboard(Share Attributes)
    else if (strcmp(it->key().c_str(), pHSensor_INTERVAL_ATTR) == 0) {
      const uint8_t pH_new_interval = it->value().as<uint8_t>();
      Serial.print("pH Interval: ");
      Serial.println(pH_new_interval);
      EEPROM.write(intervalpH_eepromAddress, pH_new_interval);
      EEPROM.commit(); // Only necessary for ESP32
    }
    // Get DO Interval from Dashboard(Share Attributes)
    else if (strcmp(it->key().c_str(), DOSensor_INTERVAL_ATTR) == 0) {
      const uint8_t DO_new_interval = it->value().as<uint8_t>();
      Serial.print("DO Interval: ");
      Serial.println(DO_new_interval);
      EEPROM.write(intervalDO_eepromAddress, DO_new_interval);
      EEPROM.commit(); // Only necessary for ESP32
    }
    // Get ORP Interval from Dashboard(Share Attributes)
    else if (strcmp(it->key().c_str(), ORPSensor_INTERVAL_ATTR) == 0) {
      const uint8_t ORP_new_interval = it->value().as<uint8_t>();
      Serial.print("ORP Interval: ");
      Serial.println(ORP_new_interval);
      EEPROM.write(intervalORP_eepromAddress, ORP_new_interval);
      EEPROM.commit(); // Only necessary for ESP32
    }
    // Get TDS Interval from Dashboard(Share Attributes)
    else if (strcmp(it->key().c_str(), TDSSensor_INTERVAL_ATTR) == 0) {
      const uint8_t TDS_new_interval = it->value().as<uint8_t>();
      Serial.print("TDS Interval: ");
      Serial.println(TDS_new_interval);
      EEPROM.write(intervalTDS_eepromAddress, TDS_new_interval);
      EEPROM.commit(); // Only necessary for ESP32
    }
    // Get TEMP Interval from Dashboard(Share Attributes)
    else if (strcmp(it->key().c_str(), TEMPSensor_INTERVAL_ATTR) == 0) {
      const uint8_t TEMP_new_interval = it->value().as<uint8_t>();
      Serial.print("TEMP Interval: ");
      Serial.println(TEMP_new_interval);
      EEPROM.write(intervalDS18B20_temp_eepromAddress, TEMP_new_interval);
      EEPROM.commit(); // Only necessary for ESP32
    }
  }
  attributesChanged = true;
}

void processClientAttributes(const Shared_Attribute_Data &data) {
  for (auto it = data.begin(); it != data.end(); ++it) {
    if (strcmp(it->key().c_str(), LED_MODE_ATTR) == 0) {
      const uint16_t new_mode = it->value().as<uint16_t>();
      ledMode = new_mode;
    }
  }
}

const Shared_Attribute_Callback
    attributes_callback(&processSharedAttributes,
                        SHARED_ATTRIBUTES_LIST.cbegin(),
                        SHARED_ATTRIBUTES_LIST.cend());
const Attribute_Request_Callback
    attribute_shared_request_callback(&processSharedAttributes,
                                      SHARED_ATTRIBUTES_LIST.cbegin(),
                                      SHARED_ATTRIBUTES_LIST.cend());
const Attribute_Request_Callback
    attribute_client_request_callback(&processClientAttributes,
                                      CLIENT_ATTRIBUTES_LIST.cbegin(),
                                      CLIENT_ATTRIBUTES_LIST.cend());

unsigned long previousMillisDS18B20_temp = 0;
unsigned long previousMillisTDS = 0;
unsigned long previousMillisORP = 0;
unsigned long previousMillispH = 0;
unsigned long previousMillisDO = 0;
unsigned long previousDisplay = 0;

const long milliseconds = 60000;     // 1min = 60000ms
const long intervalDS18B20_temp = 2; // Interval for pH sensor in Minutes
const long intervalTDS = 4;          // Interval for TDS sensor in Minutes
const long intervalORP = 5;          // Interval for ORP sensor in Minutes
// uint8_t intervalpH; // Interval for pH sensor in Minutes
const long intervalpH = 1; // Interval for pH sensor in Minutes
const long intervalDO = 3; // Interval for DO sensor in Minutes
const long intervalDisplay = 1;

void setup() {
  // Initalize serial connection for debugging
  Serial.begin(SERIAL_DEBUG_BAUD);
  pinMode(buttonPin, INPUT_PULLUP);
  EEPROM.begin(32);

  if (LED_BUILTIN != 99) {
    pinMode(LED_BUILTIN, OUTPUT);
  }
  delay(1000);
  InitWiFi();
  ph.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }
}

void loop() {
  unsigned long currentMillis = millis();
  delay(10);

  int reading = digitalRead(buttonPin);
  // Check for button state changes
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    // If the button state has changed, update the current state
    if (reading != buttonState) {
      buttonState = reading;
      Serial.print("Onboard Button State before");
      Serial.println(buttonState);
      // Print the button state to the serial monitor
      if (buttonState == LOW) {
        // if (Ext_buttonState == 1) {
        Serial.print("Onboard Button State after");
        Serial.println(buttonState);
        Serial.println("Button is pressed");
        Serial.println(blinkingInterval);

        Serial.println("pH Interval");
        Serial.println(EEPROM.read(intervalpH_eepromAddress));
        Serial.println("DO Interval");
        Serial.println(EEPROM.read(intervalDO_eepromAddress));
        Serial.println("TDS Interval");
        Serial.println(EEPROM.read(intervalTDS_eepromAddress));
        Serial.println("ORP Interval");
        Serial.println(EEPROM.read(intervalORP_eepromAddress));
        Serial.println("TEMP Interval");
        Serial.println(EEPROM.read(intervalDS18B20_temp_eepromAddress));

        tb.sendTelemetryData("Button", 1); // send to TB
        display.clearDisplay();
        display.drawRect(0, 0, display.width(), display.height(),
                         SSD1306_WHITE);
        display.drawRect(0, 0, display.width(), 12, SSD1306_WHITE);
        display.setTextSize(1);
        display.setCursor(5, 2);
        display.print(F("Name       Value"));
        display.setCursor(5, 14);
        display.print(F("Temp(C)         "));
        display.setCursor(5, 24);
        display.print(F("TDS(ppm)          "));
        display.setCursor(5, 34);
        display.print(F("pH        "));
        display.setCursor(5, 44);
        display.print(F("ORP(mV)       "));
        display.setCursor(5, 54);
        display.print(F("DO(mg/L)       "));
        // Display Temp
        float temperature = getTemp();
        Serial.println("temperature");
        Serial.println(temperature);
        tb.sendTelemetryData("TEMP", temperature);
        display.setCursor(71, 14);
        display.print(temperature, 2);

        // Display TDS
        float tdsValue = getTdsValue(); // Get TDS sensor Data
        Serial.println(tdsValue, 4);
        tb.sendTelemetryData("TDS", tdsValue); // send to TB
        display.setCursor(71, 24);
        display.print(tdsValue, 2);

        // Display pH
        voltage = analogRead(PH_PIN) / ADC_RES * VREF; // read the voltage
        // ph.begin();
        phValue =
            (ph.readPH(voltage, temperature_pH)) +
            pH_cali; // convert voltage to pH with temperature compensation
        tb.sendTelemetryData("pH", phValue); // send to TB
        Serial.println(phValue, 4);
        display.setCursor(71, 34);
        display.print(phValue, 2);

        // Display ORP
        double orp_sensor_Value = loopORP_sensor2();
        Serial.println(orp_sensor_Value, 4);
        tb.sendTelemetryData("ORP", orp_sensor_Value); // send to TB
        display.setCursor(71, 44);
        display.print(orp_sensor_Value, 2);

        // Display DO
        // uint8_t Temperaturet = (uint8_t)temperature;
        // uint16_t ADC_Raw = analogRead(doxygen.getDO_PIN());
        // uint16_t ADC_Voltage = (uint32_t)VREF * ADC_Raw / ADC_RES;
        // float DO_Value = ((doxygen.readDO(ADC_Voltage, Temperaturet)) / 1000)
        // - DoValue_cali;
        Temperaturet = (uint8_t)READ_TEMP;
        ADC_Raw = analogRead(DO_PIN);
        ADC_Voltage = uint32_t(VREF) * ADC_Raw / ADC_RES;
        float DO_Value =
            ((readDO(ADC_Voltage, Temperaturet)) / 1000.0) + DO_cali;
        Serial.println(DO_Value, 4);
        tb.sendTelemetryData("DO", DO_Value); // send to TB
        display.setCursor(71, 54);
        display.print(DO_Value, 2);

        display.display();
      }
      // }
      else {
        Serial.println("Button is released");
        display.clearDisplay();

        display.setTextSize(2);
        display.setTextColor(WHITE);
        display.setCursor(0, 0);
        // Display static text
        display.println("iSGM");
        display.setTextSize(1.5);
        display.setTextColor(WHITE);
        display.setCursor(0, 25);
        // Display static text
        display.println("Water Quality Sensor Kit");
        display.setTextSize(1.5);
        display.setTextColor(WHITE);
        display.setCursor(0, 50);
        // Display static text
        display.println("Nov2023_V1.1");
        display.display();
      }
    }
  }

  if (!reconnect()) {
    return;
  }

  if (!tb.connected()) {
    // Connect to the ThingsBoard
    Serial.print("Connecting to: ");
    Serial.print(THINGSBOARD_SERVER);
    Serial.print(" with token ");
    Serial.println(TOKEN);
    if (!tb.connect(THINGSBOARD_SERVER, TOKEN, THINGSBOARD_PORT)) {
      Serial.println("Failed to connect");
      return;
    }
    // Sending a MAC address as an attribute
    tb.sendAttributeData("macAddress", WiFi.macAddress().c_str());

    Serial.println("Subscribing for RPC...");
    // Perform a subscription. All consequent data processing will happen in
    // processSetLedState() and processSetLedMode() functions,
    // as denoted by callbacks array.
    if (!tb.RPC_Subscribe(callbacks.cbegin(), callbacks.cend())) {
      Serial.println("Failed to subscribe for RPC");
      return;
    }

    if (!tb.Shared_Attributes_Subscribe(attributes_callback)) {
      Serial.println("Failed to subscribe for shared attribute updates");
      return;
    }

    Serial.println("Subscribe done");

    // Request current states of shared attributes
    if (!tb.Shared_Attributes_Request(attribute_shared_request_callback)) {
      Serial.println("Failed to request for shared attributes");
      return;
    }

    // Request current states of client attributes
    if (!tb.Client_Attributes_Request(attribute_client_request_callback)) {
      Serial.println("Failed to request for client attributes");
      return;
    }
  }

  if (attributesChanged) {
    attributesChanged = false;
    if (ledMode == 0) {
      previousStateChange = millis();
    }
    tb.sendTelemetryData(LED_MODE_ATTR, ledMode);
    tb.sendTelemetryData(LED_STATE_ATTR, ledState);
    tb.sendAttributeData(LED_MODE_ATTR, ledMode);
    tb.sendAttributeData(LED_STATE_ATTR, ledState);
  }

  if (ledMode == 1 && millis() - previousStateChange > blinkingInterval) {
    previousStateChange = millis();
    ledState = !ledState;
    tb.sendTelemetryData(LED_STATE_ATTR, ledState);
    tb.sendAttributeData(LED_STATE_ATTR, ledState);
    if (LED_BUILTIN == 99) {
      Serial.print("LED state changed to: ");
      Serial.println(ledState);
    } else {
      digitalWrite(LED_BUILTIN, ledState);
    }
  }

  // Sending telemetry every telemetrySendInterval time
  if (currentMillis - previousMillisDS18B20_temp >=
      EEPROM.read(intervalDS18B20_temp_eepromAddress) * milliseconds) {
    previousMillisDS18B20_temp = currentMillis;
    // Get DS18B20 Data
    // float DS18B20_temp = tempSensor.getTemperature(); // Get DS18B20 sensor
    // Data
    float temperature = getTemp();
    Serial.println("temperature");
    Serial.println(temperature);
    tb.sendTelemetryData("TEMP", temperature);
    tb.sendAttributeData("rssi", WiFi.RSSI());
    tb.sendAttributeData("channel", WiFi.channel());
    tb.sendAttributeData("bssid", WiFi.BSSIDstr().c_str());
    tb.sendAttributeData("localIp", WiFi.localIP().toString().c_str());
    tb.sendAttributeData("ssid", WiFi.SSID().c_str());
  }

  // Sending telemetry every telemetrySendInterval time
  if (currentMillis - previousMillisTDS >=
      EEPROM.read(intervalTDS_eepromAddress) * milliseconds) {
    previousMillisTDS = currentMillis;
    // Get TDS sensor Data
    float tdsValue = getTdsValue(); // Get TDS sensor Data
    tb.sendTelemetryData("TDS", tdsValue);
    tb.sendAttributeData("rssi", WiFi.RSSI());
    tb.sendAttributeData("channel", WiFi.channel());
    tb.sendAttributeData("bssid", WiFi.BSSIDstr().c_str());
    tb.sendAttributeData("localIp", WiFi.localIP().toString().c_str());
    tb.sendAttributeData("ssid", WiFi.SSID().c_str());
  }

  if (currentMillis - previousMillispH >=
      EEPROM.read(intervalpH_eepromAddress) * milliseconds) {
    previousMillispH = currentMillis;

    // Get pH Sensor Data
    voltage = analogRead(PH_PIN) / ADC_RES * VREF; // read the voltage
    // ph.begin();
    phValue = (ph.readPH(voltage, temperature_pH)) +
              pH_cali; // convert voltage to pH with temperature compensation
    tb.sendTelemetryData("pH", phValue);
    tb.sendAttributeData("rssi", WiFi.RSSI());
    tb.sendAttributeData("channel", WiFi.channel());
    tb.sendAttributeData("bssid", WiFi.BSSIDstr().c_str());
    tb.sendAttributeData("localIp", WiFi.localIP().toString().c_str());
    tb.sendAttributeData("ssid", WiFi.SSID().c_str());
    Serial.println("phValue");
    Serial.println(phValue);
  }

  if (currentMillis - previousMillisORP >=
      EEPROM.read(intervalORP_eepromAddress) * milliseconds) {
    previousMillisORP = currentMillis;
    // Get ORP sensor data
    double orp_sensor_Value = loopORP_sensor2();
    tb.sendTelemetryData("ORP", orp_sensor_Value);
    tb.sendAttributeData("rssi", WiFi.RSSI());
    tb.sendAttributeData("channel", WiFi.channel());
    tb.sendAttributeData("bssid", WiFi.BSSIDstr().c_str());
    tb.sendAttributeData("localIp", WiFi.localIP().toString().c_str());
    tb.sendAttributeData("ssid", WiFi.SSID().c_str());
  }

  if (currentMillis - previousMillisDO >=
      EEPROM.read(intervalDO_eepromAddress) * milliseconds) {
    previousMillisDO = currentMillis;

    Temperaturet = (uint8_t)READ_TEMP;
    ADC_Raw = analogRead(DO_PIN);
    ADC_Voltage = uint32_t(VREF) * ADC_Raw / ADC_RES;
    float DO_Value = ((readDO(ADC_Voltage, Temperaturet)) / 1000.0) + DO_cali;
    tb.sendTelemetryData("DO", DO_Value);
    tb.sendAttributeData("rssi", WiFi.RSSI());
    tb.sendAttributeData("channel", WiFi.channel());
    tb.sendAttributeData("bssid", WiFi.BSSIDstr().c_str());
    tb.sendAttributeData("localIp", WiFi.localIP().toString().c_str());
    tb.sendAttributeData("ssid", WiFi.SSID().c_str());
  }

  // Update the last button state
  lastButtonState = reading;

  tb.loop();
}

float getTemp() {
  // returns the temperature from one DS18S20 in DEG Celsius

  byte data[12];
  byte addr[8];

  if (!ds.search(addr)) {
    // no more sensors on chain, reset search
    ds.reset_search();
    return -1000;
  }

  if (OneWire::crc8(addr, 7) != addr[7]) {
    Serial.println("CRC is not valid!");
    return -1000;
  }

  if (addr[0] != 0x10 && addr[0] != 0x28) {
    Serial.print("Device is not recognized");
    return -1000;
  }

  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1); // start conversion, with parasite power on at the end

  byte present = ds.reset();
  ds.select(addr);
  ds.write(0xBE); // Read Scratchpad

  for (int i = 0; i < 9; i++) { // we need 9 bytes
    data[i] = ds.read();
  }

  ds.reset_search();

  byte MSB = data[1];
  byte LSB = data[0];

  float tempRead = ((MSB << 8) | LSB); // using two's compliment
  float TemperatureSum = tempRead / 16;

  return TemperatureSum;
}