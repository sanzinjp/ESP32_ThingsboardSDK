#include <Arduino_MQTT_Client.h>
#include <DHT.h>
#include <ThingsBoard.h>
#include <WiFi.h>


// WiFi credentials
const char *ssid = "TP-Link_60_502";
const char *password = "isgm1234";

// ThingsBoard device credentials
constexpr char TOKEN[] = "vyT28nuMgr7DLU5W0SkH";
constexpr char THINGSBOARD_SERVER[] = "thingsboard.cloud";
constexpr uint16_t THINGSBOARD_PORT = 1883;

// DHT sensor configuration
#define DHTPIN 21
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// MQTT and ThingsBoard client configuration
WiFiClient wifiClient;
Arduino_MQTT_Client mqttClient(wifiClient);
ThingsBoard tb(mqttClient);

// Telemetry keys
constexpr char TEMPERATURE_KEY[] = "temperature";
constexpr char HUMIDITY_KEY[] = "humidity";

// Connect to WiFi
void connectToWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
}

// Reconnect if WiFi is disconnected
bool ensureWiFiConnected() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }
  connectToWiFi();
  return true;
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  connectToWiFi();
}

void loop() {
  // Read temperature and humidity
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read from DHT sensor!");
    delay(2000);
    return;
  }

  // Ensure WiFi connection
  if (!ensureWiFiConnected()) {
    return;
  }

  // Connect to ThingsBoard if not connected
  if (!tb.connected()) {
    Serial.println("Connecting to ThingsBoard...");
    if (!tb.connect(THINGSBOARD_SERVER, TOKEN, THINGSBOARD_PORT)) {
      Serial.println("ThingsBoard connection failed");
      delay(5000);
      return;
    }
  }

  // Send telemetry
  Serial.println("Sending telemetry data...");
  tb.sendTelemetryData(TEMPERATURE_KEY, temperature);
  tb.sendTelemetryData(HUMIDITY_KEY, humidity);

  // Log to Serial
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.print(" °C, Humidity: ");
  Serial.print(humidity);
  Serial.println(" %");

  tb.loop();    // Keep MQTT connection alive
  delay(20000); // Send every 20 seconds
}
