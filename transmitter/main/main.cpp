#include <stdio.h>
#include <nvs_flash.h>
#include <stdbool.h>
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"
#include "esp_err.h"
#include "string.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "wcan_communication.h"
#include "wcan_utils.h"

uint16_t allowed_ids[3] = {0x123, 0x456, 0x789};
#define ALLOWED_IDS_SIZE (sizeof(allowed_ids) / sizeof(allowed_ids[0]))

static void ReadDataTask(void *pvParameter){
    static const char *TAG = "READ";
    ESP_LOGI(TAG, "Read data task started");

    while(1) {
        for (int i = 0; i < ALLOWED_IDS_SIZE; i++) {
            data_packet_t send_data;
            send_data.can_id = allowed_ids[i];
            send_data.payload = NULL;
            send_data.payload_len = 0;
            ESP_LOGI(TAG, "Reading data for %04x", send_data.can_id);

            switch (send_data.can_id){
            case 0x123:{
                float f_data = 3.14f;
                send_data.payload_len = sizeof(f_data);
                send_data.payload = (uint8_t *)malloc(send_data.payload_len);
                if (send_data.payload == NULL) {
                    ESP_LOGE(TAG, "Malloc payload fail");
                    break;
                }
                memcpy(send_data.payload, &f_data, send_data.payload_len);
                ESP_LOGI(TAG, "[%04x] %f", send_data.can_id, f_data);
                break;
            }
            case 0x456:{
                int32_t i_data = 42;
                send_data.payload_len = sizeof(i_data);
                send_data.payload = (uint8_t *)malloc(send_data.payload_len);
                if (send_data.payload == NULL) {
                    ESP_LOGE(TAG, "Malloc payload fail");
                    break;
                }
                memcpy(send_data.payload, &i_data, send_data.payload_len);
                ESP_LOGI(TAG, "[%04x] %ld", send_data.can_id, i_data);
                break;
            }
            case 0x789:{
                char str_data[] = "Hello";
                send_data.payload_len = strlen(str_data) + 1;
                send_data.payload = (uint8_t *)malloc(send_data.payload_len);
                if (send_data.payload == NULL) {
                    ESP_LOGE(TAG, "Malloc payload fail");
                    break;
                }
                send_data.payload[0] = send_data.payload_len-1;
                memcpy(send_data.payload + 1, str_data, send_data.payload_len);
                ESP_LOGI(TAG, "[%04x] %s", send_data.can_id, str_data);
                break;
            }
            default:
                ESP_LOGE(TAG, "[%04x] Unknown", send_data.can_id);
                continue;
            }

            if (send_data.payload == NULL) continue;

            if (xQueueSend(send_queue, &send_data, ESPNOW_MAXDELAY) == pdTRUE) {
                ESP_LOGI(TAG, "Data read successfully");
            } else {
                ESP_LOGW(TAG, "Send send queue fail");
                free(send_data.payload);
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
    vTaskDelete(NULL);
}

void RecvCallback(data_packet_t data)
{
    static const char *TAG = "USER-RECV";
    switch (data.can_id)
    {
    case 0x123:{
        float float_data = *(float *)(data.payload);
        ESP_LOGI(TAG, "[%04x] %f", data.can_id, float_data);
        break;
    }
    case 0x456:{
        int32_t int_data = *(int32_t *)(data.payload);
        ESP_LOGI(TAG, "[%04x] %ld", data.can_id, int_data);
        break;
    }
    case 0x789:{
        uint8_t str_len = *(int8_t *)(data.payload);
        char *str_data = (char *)(data.payload + 1);
        str_data[str_len] = '\0'; // Null-terminate the string
        ESP_LOGI(TAG, "[%04x] %s", data.can_id, str_data);
        break;
    }
    default:
        ESP_LOGE(TAG, "[%04x] Unknown", data.can_id);
        PrintCharPacket(data.payload, data.payload_len);
        break;
    }
}

static void WiFiInit(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(ESPNOW_WIFI_MODE));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_ERROR_CHECK(esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));
}

extern "C" void app_main(void){
    static const char *TAG = "MAIN";
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    WiFiInit();
    ESP_LOGI(TAG, "WiFi initialized");
    WCAN_Init(true, allowed_ids, ALLOWED_IDS_SIZE);
    ESP_LOGI(TAG, "Setup completed");

    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_STA));
    char *mac_str = MacToString(mac);
    ESP_LOGI(TAG, "MAC address: %s", mac_str);
    free(mac_str);
    uint8_t target_mac[6] = {0x54, 0x32, 0x04, 0x8c, 0x0b, 0x8c};
    if (memcmp(mac, target_mac, sizeof(mac)) == 0) {
        ESP_LOGI(TAG, "This is a master device");
    } else {
        ESP_LOGI(TAG, "This is a sensor device, starting read task");
        xTaskCreate(ReadDataTask, "ReadDataTask", 4096, NULL, 5, NULL);
    }
}