#include <cstring>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_ota_ops.h>
#include <esp_wifi.h>
#include <nvs_flash.h>


#include <Espressif_MQTT_Client.h>
#include <OTA_Firmware_Update.h>
#include <SDCard_Updater.h>
#include <ThingsBoard.h>


#define ENCRYPTED false

constexpr char WIFI_SSID[] = "YOUR_WIFI_SSID";
constexpr char WIFI_PASSWORD[] = "YOUR_WIFI_PASSWORD";
constexpr char TOKEN[] = "YOUR_DEVICE_ACCESS_TOKEN";
constexpr char THINGSBOARD_SERVER[] = "demo.thingsboard.io";
constexpr uint16_t THINGSBOARD_PORT = ENCRYPTED ? 8883U : 1883U;

constexpr char CURRENT_FIRMWARE_TITLE[] = "ESP32";
constexpr char CURRENT_FIRMWARE_VERSION[] = "1.0.0";
constexpr uint8_t FIRMWARE_FAILURE_RETRIES = 12U;
constexpr uint16_t FIRMWARE_PACKET_SIZE = 4096U;
constexpr uint16_t MAX_MESSAGE_SEND_SIZE = FIRMWARE_PACKET_SIZE + 50U;
constexpr uint16_t MAX_MESSAGE_RECEIVE_SIZE = FIRMWARE_PACKET_SIZE + 50U;
constexpr char UPDATE_FILE_PATH[] = "/sd/update.bin";

#if ENCRYPTED
constexpr char ROOT_CERT[] = R"(-----BEGIN CERTIFICATE-----
... (your certificate here)
-----END CERTIFICATE-----)";
#endif

Espressif_MQTT_Client<> mqttClient;
OTA_Firmware_Update<> ota;
const std::array<IAPI_Implementation *, 1U> apis = {&ota};
ThingsBoard tb(mqttClient, MAX_MESSAGE_RECEIVE_SIZE, MAX_MESSAGE_SEND_SIZE,
               Default_Max_Stack_Size, apis);
SDCard_Updater<> updater(UPDATE_FILE_PATH);

bool wifi_connected = false;
bool currentFWSent = false;
bool updateRequestSent = false;

struct binary_data_t {
  size_t size;
  size_t remaining_size;
  void *data;
};

void otaSDToFlashTask(void *pvParameter) {
  FILE *file = fopen(UPDATE_FILE_PATH, "rb");
  if (!file) {
    ESP_LOGE("OTA", "File open failed");
    vTaskDelete(NULL);
  }

  esp_ota_handle_t handle;
  const esp_partition_t *partition = esp_ota_get_next_update_partition(NULL);
  binary_data_t data;

  fseek(file, 0, SEEK_END);
  data.size = ftell(file);
  data.remaining_size = data.size;
  fseek(file, 0, SEEK_SET);
  data.data = malloc(FIRMWARE_PACKET_SIZE);
  esp_ota_begin(partition, OTA_SIZE_UNKNOWN, &handle);

  while (data.remaining_size > 0) {
    size_t size = std::min(FIRMWARE_PACKET_SIZE, data.remaining_size);
    fread(data.data, size, 1, file);
    esp_ota_write(handle, data.data, size);
    data.remaining_size -= size;
  }

  fclose(file);
  esp_ota_end(handle);
  esp_ota_set_boot_partition(partition);
  ESP_LOGI("OTA", "Flashed successfully. Restarting...");
  esp_restart();
}

void update_starting_callback() {}
void finished_callback(const bool &success) {
  if (success) {
    ESP_LOGI("OTA", "Firmware saved to SD. Flashing...");
    xTaskCreate(otaSDToFlashTask, "OTA_SD_TO_FLASH",
                FIRMWARE_PACKET_SIZE + 1024, NULL, 16, NULL);
  } else {
    ESP_LOGW("OTA", "Firmware download failed");
  }
}

void progress_callback(const size_t &current, const size_t &total) {
  ESP_LOGI("OTA", "Progress: %.2f%%", (float)current * 100.0f / total);
}

void on_got_ip(void *, esp_event_base_t, int32_t, void *) {
  wifi_connected = true;
}

void InitWiFi() {
  const wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);

  esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_WIFI_STA();
  esp_netif_t *netif = esp_netif_new(&netif_cfg);
  esp_netif_attach_wifi_station(netif);

  esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_got_ip, NULL);
  esp_wifi_set_default_wifi_sta_handlers();
  esp_wifi_set_storage(WIFI_STORAGE_RAM);

  wifi_config_t wifi_cfg{};
  strncpy((char *)wifi_cfg.sta.ssid, WIFI_SSID, sizeof(wifi_cfg.sta.ssid));
  strncpy((char *)wifi_cfg.sta.password, WIFI_PASSWORD,
          sizeof(wifi_cfg.sta.password));

  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
  esp_wifi_start();
  esp_wifi_connect();
}

extern "C" void app_main() {
  ESP_LOGI("APP", "Startup - Free heap: %u bytes", esp_get_free_heap_size());
  nvs_flash_init();
  esp_netif_init();
  esp_event_loop_create_default();

  InitWiFi();
#if ENCRYPTED
  mqttClient.set_server_certificate(ROOT_CERT);
#endif

  while (true) {
    if (!wifi_connected) {
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      continue;
    }

    if (!tb.connected()) {
      tb.connect(THINGSBOARD_SERVER, TOKEN, THINGSBOARD_PORT);
    }

    if (!currentFWSent) {
      currentFWSent = ota.Firmware_Send_Info(CURRENT_FIRMWARE_TITLE,
                                             CURRENT_FIRMWARE_VERSION);
    }

    if (!updateRequestSent) {
      const OTA_Update_Callback callback(
          CURRENT_FIRMWARE_TITLE, CURRENT_FIRMWARE_VERSION, &updater,
          &finished_callback, &progress_callback, &update_starting_callback,
          FIRMWARE_FAILURE_RETRIES, FIRMWARE_PACKET_SIZE);
      updateRequestSent = ota.Start_Firmware_Update(callback);
    }

    tb.loop();
    vTaskDelay(10000 / portTICK_PERIOD_MS);
  }
}
