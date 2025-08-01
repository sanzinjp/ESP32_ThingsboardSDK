#include <Arduino_MQTT_Client.h>
#include <Server_Side_RPC.h>
#include <ThingsBoard.h>
#include <WiFi.h>

constexpr char WIFI_SSID[] = "TP-Link_60_502";
constexpr char WIFI_PASSWORD[] = "isgm1234";
constexpr char TOKEN[] = "vyT28nuMgr7DLU5W0SkH";
constexpr char THINGSBOARD_SERVER[] = "thingsboard.cloud";
constexpr uint16_t THINGSBOARD_PORT = 1883U;
constexpr uint16_t MAX_MESSAGE_SEND_SIZE = 128U;
constexpr uint16_t MAX_MESSAGE_RECEIVE_SIZE = 128U;
constexpr uint32_t SERIAL_DEBUG_BAUD = 115200U;

// RPC method name
constexpr const char RPC_SET_STATE[] = "setState";
constexpr uint8_t MAX_RPC_SUBSCRIPTIONS = 1U;
constexpr uint8_t MAX_RPC_RESPONSE = 5U;

// WiFi client and MQTT
WiFiClient espClient;
Arduino_MQTT_Client mqttClient(espClient);

// Server-Side RPC
Server_Side_RPC<MAX_RPC_SUBSCRIPTIONS, MAX_RPC_RESPONSE> rpc;
const std::array<IAPI_Implementation *, 1U> apis = {&rpc};

// ThingsBoard client
ThingsBoard tb(mqttClient, MAX_MESSAGE_RECEIVE_SIZE, MAX_MESSAGE_SEND_SIZE,
               Default_Max_Stack_Size, apis);

bool subscribed = false;

// WiFi initialization
void InitWiFi() {
  Serial.println("Connecting to AP ...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to AP");
}

// WiFi reconnect
bool reconnect() {
  if (WiFi.status() != WL_CONNECTED) {
    InitWiFi();
  }
  return WiFi.status() == WL_CONNECTED;
}

// === RPC callback implementation ===
void processSetState(const JsonVariantConst &data, JsonDocument &response) {
  Serial.println("Received RPC: setState");

  const char *command = data["command"];
  if (command) {
    Serial.print("Command: ");
    Serial.println(command);

    if (strcmp(command, "getCurrentData") == 0) {
      response["temperature"] = 24.3;
      response["humidity"] = 55;
    } else {
      response["error"] = "Unknown command";
    }
  } else {
    response["error"] = "Missing 'command' parameter";
  }
}

void setup() {
  Serial.begin(SERIAL_DEBUG_BAUD);
  delay(1000);

  InitWiFi();
}

void loop() {
  delay(100);

  if (!reconnect())
    return;

  if (!tb.connected()) {
    Serial.println("Connecting to ThingsBoard...");
    if (!tb.connect(THINGSBOARD_SERVER, TOKEN, THINGSBOARD_PORT)) {
      Serial.println("Failed to connect");
      return;
    }
  }

  if (!subscribed) {
    Serial.println("Subscribing to RPC...");
    const RPC_Callback callbacks[] = {{RPC_SET_STATE, processSetState}};

    if (!rpc.RPC_Subscribe(std::begin(callbacks), std::end(callbacks))) {
      Serial.println("Failed to subscribe to RPC");
      return;
    }

    Serial.println("RPC subscribed");
    subscribed = true;
  }

  tb.loop();
}
