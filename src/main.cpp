#include <Arduino_MQTT_Client.h>
#include <Preferences.h>
#include <Shared_Attribute_Update.h>
#include <ThingsBoard.h>
#include <WiFi.h>

#define LED 2 // Define the LED pin, change as needed

#define ENCRYPTED false

constexpr char WIFI_SSID[] = "TP-Link_60_502";
constexpr char WIFI_PASSWORD[] = "isgm1234";

constexpr char TOKEN[] = "vyT28nuMgr7DLU5W0SkH";

// Thingsboard we want to establish a connection too
constexpr char THINGSBOARD_SERVER[] = "thingsboard.cloud";

#if ENCRYPTED
constexpr uint16_t THINGSBOARD_PORT = 8883U;
#else
constexpr uint16_t THINGSBOARD_PORT = 1883U;
#endif

constexpr uint16_t MAX_MESSAGE_SEND_SIZE = 128U;
constexpr uint16_t MAX_MESSAGE_RECEIVE_SIZE = 128U;

constexpr uint32_t SERIAL_DEBUG_BAUD = 115200U;

constexpr size_t MAX_ATTRIBUTES = 6U;

constexpr char CONNECTING_MSG[] = "Connecting to: (%s) with token (%s)\n";
char constexpr WIFI_USERNAME_KEY[] = "wifi_username_key";
char constexpr WIFI_PASSWORD_KEY[] = "wifi_password_key";
constexpr char LED_BLINK_INTERVAL_KEY[] = "led_blink_interval";
uint16_t blinkIntervalMs = 500; // default value

// Initialize underlying client, used to establish a connection
#if ENCRYPTED
WiFiClientSecure espClient;
#else
WiFiClient espClient;
#endif

Preferences preferences;
// Initalize the Mqtt client instance
Arduino_MQTT_Client mqttClient(espClient);
// Initialize used apis
Shared_Attribute_Update<1U, MAX_ATTRIBUTES> shared_update;
const std::array<IAPI_Implementation *, 1U> apis = {&shared_update};
// Initialize ThingsBoard instance with the maximum needed buffer size
ThingsBoard tb(mqttClient, MAX_MESSAGE_RECEIVE_SIZE, MAX_MESSAGE_SEND_SIZE,
               Default_Max_Stack_Size, apis);

// Statuses for subscribing to shared attributes
bool subscribed = false;

void InitWiFi() {
  Serial.println("Connecting to AP ...");
  // Attempting to establish a connection to the given WiFi network
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    // Delay 500ms until a connection has been successfully established
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected to AP");
#if ENCRYPTED
  espClient.setCACert(ROOT_CERT);
#endif
}

bool reconnect() {
  // Check to ensure we aren't connected yet
  const wl_status_t status = WiFi.status();
  if (status == WL_CONNECTED) {
    return true;
  }

  // If we aren't establish a new connection to the given WiFi network
  InitWiFi();
  return true;
}

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
    } else if (key == WIFI_USERNAME_KEY) {
      preferences.putString("wifi_user", it->value().as<String>());
      Serial.println("Saved wifi_user");
    } else if (key == WIFI_PASSWORD_KEY) {
      preferences.putString("wifi_pass", it->value().as<String>());
      Serial.println("Saved wifi_pass");
    }
  }

  preferences.end();
}

void setup() {
  pinMode(LED, OUTPUT);
  Serial.begin(SERIAL_DEBUG_BAUD);
  delay(1000);

  preferences.begin("tb-config", true); // read-only
  String wifiUserName = preferences.getString("wifi_user", "none");
  String wifiPass = preferences.getString("wifi_pass", "none");

  // Load blinkInterval only if it was previously saved (non-zero)
  uint16_t storedInterval = preferences.getUInt("blink_intv", 0);
  Serial.print("storedInterval: ");
  Serial.println(storedInterval);

  if (storedInterval != 0) {
    blinkIntervalMs = storedInterval;
  } else {
    blinkIntervalMs = 500;
  }

  preferences.end();

  Serial.print("Stored WiFi Username: ");
  Serial.println(wifiUserName);
  Serial.print("Stored WiFi Password: ");
  Serial.println(wifiPass);
  Serial.print("Using LED Blink Interval: ");
  Serial.println(blinkIntervalMs);

  InitWiFi();
}

unsigned long lastBlink = 0;
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
        SUBSCRIBED_SHARED_ATTRIBUTES = {WIFI_USERNAME_KEY, WIFI_PASSWORD_KEY,
                                        LED_BLINK_INTERVAL_KEY};

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
}
