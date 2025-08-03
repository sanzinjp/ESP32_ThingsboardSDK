#include <Arduino.h>
#include <Preferences.h>

// Pin definitions
#define LED 2         // Onboard or external LED pin
#define DHTPIN 21     // GPIO pin connected to the DHT sensor
#define DHTTYPE DHT22 // Type of DHT sensor used

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET -1    // Reset pin not used

#define OLED_SDA 13 // I2C SDA pin for OLED display
#define OLED_SCL 14 // I2C SCL pin for OLED display

// Global variables (declared elsewhere and defined in one .cpp file)
extern Preferences preferences; // NVS (EEPROM-like) storage handler

// extern Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire,
// OLED_RESET); // NVS (EEPROM-like) storage handler

// Firmware information
constexpr char CURRENT_FIRMWARE_TITLE[] = "ESP32-S3_Firmware";
constexpr char CURRENT_FIRMWARE_VERSION[] = "1.0.3";
constexpr uint8_t FIRMWARE_FAILURE_RETRIES = 12U;
constexpr uint16_t FIRMWARE_PACKET_SIZE = 4096U;

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

// RPC method name
constexpr const char RPC_SET_STATE[] = "setState";
constexpr uint8_t MAX_RPC_SUBSCRIPTIONS = 1U;
constexpr uint8_t MAX_RPC_RESPONSE = 5U;

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

extern float temp; // Temperature reading from DHT sensor
extern float hum;  // Humidity reading from DHT sensor
