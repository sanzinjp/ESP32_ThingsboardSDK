#include <Arduino.h>
#include <Preferences.h>

// Pin definitions
#define LED 2         // Onboard or external LED pin
#define DHTPIN 21     // GPIO pin connected to the DHT sensor
#define DHTTYPE DHT22 // Type of DHT sensor used

// Global variables (declared elsewhere and defined in one .cpp file)
extern Preferences preferences; // NVS (EEPROM-like) storage handler

// WiFi credentials
constexpr char WIFI_SSID[] = "TP-Link_60_502"; // Replace with your actual SSID
constexpr char WIFI_PASSWORD[] =
    "isgm1234"; // Replace with your actual password

// Token used to authenticate with ThingsBoard
constexpr char TOKEN[] = "vyT28nuMgr7DLU5W0SkH";

// ThingsBoard server address
constexpr char THINGSBOARD_SERVER[] = "thingsboard.cloud";

// MQTT port configuration (depending on encryption setting)
#if ENCRYPTED
constexpr uint16_t THINGSBOARD_PORT = 8883U; // MQTT over TLS
#else
constexpr uint16_t THINGSBOARD_PORT = 1883U; // Plain MQTT
#endif

// MQTT message buffer sizes
constexpr uint16_t MAX_MESSAGE_SEND_SIZE = 128U;
constexpr uint16_t MAX_MESSAGE_RECEIVE_SIZE = 128U;

// Serial port baud rate for debugging output
constexpr uint32_t SERIAL_DEBUG_BAUD = 115200U;

// Maximum number of shared attributes expected from ThingsBoard
constexpr size_t MAX_ATTRIBUTES = 3U;

// Logging message format for ThingsBoard connection
constexpr char CONNECTING_MSG[] = "Connecting to: (%s) with token (%s)\n";

// Attribute keys for shared attribute updates from ThingsBoard
constexpr char LED_BLINK_INTERVAL_KEY[] =
    "led_blink_interval"; // Key to update LED blink rate
constexpr char DHT22_INTERVAL_KEY[] =
    "dht22_interval"; // Key to update DHT22 telemetry interval

// Shared variables that are updated at runtime via shared attribute
// subscription
extern uint16_t blinkIntervalMs; // LED blinking interval (milliseconds)
extern uint16_t
    dht22IntervalMs; // DHT22 telemetry sending interval (milliseconds)
