#include <stdio.h>
#include <nvs_flash.h>
#include <stdbool.h>

#include "esp_log.h"
#include "esp_err.h"
#include "string.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "wcan_communication.h"

static const char *TAG = "MAIN";

static void ReadDataTask(void *pvParameter){
    ESP_LOGI(TAG, "Read data task started");

    while(1) {
        for (int i = 0; i < ALLOWED_IDS_SIZE; i++) {
            event_send_cb_t send_cb;
            send_cb.can_id = recv_allowed_ids[i];
            send_cb.payload = NULL;
            send_cb.payload_len = 0;
            ESP_LOGI(TAG, "Reading data for %04x", send_cb.can_id);
            switch (send_cb.can_id){
            case 0x123:{
                float f_data = 3.14f;
                send_cb.payload_len = sizeof(f_data);
                send_cb.payload = (uint8_t *)malloc(send_cb.payload_len);
                if (send_cb.payload == NULL) {
                    ESP_LOGE(TAG, "Malloc payload fail");
                    break;
                }
                memcpy(send_cb.payload, &f_data, send_cb.payload_len);
                ESP_LOGI(TAG, "Float data is: %f", f_data);
                break;
            }
            case 0x456:{
                int32_t i_data = 42;
                send_cb.payload_len = sizeof(i_data);
                send_cb.payload = (uint8_t *)malloc(send_cb.payload_len);
                if (send_cb.payload == NULL) {
                    ESP_LOGE(TAG, "Malloc payload fail");
                    break;
                }
                memcpy(send_cb.payload, &i_data, send_cb.payload_len);
                ESP_LOGI(TAG, "Int data is: %ld", i_data);
                break;
            }
            case 0x789:{
                char str_data[] = "Hello";
                send_cb.payload_len = strlen(str_data) + 1;
                send_cb.payload = (uint8_t *)malloc(send_cb.payload_len);
                if (send_cb.payload == NULL) {
                    ESP_LOGE(TAG, "Malloc payload fail");
                    break;
                }
                send_cb.payload[0] = send_cb.payload_len-1;
                memcpy(send_cb.payload + 1, str_data, send_cb.payload_len);
                ESP_LOGI(TAG, "String data is: %s", str_data);
                break;
            }
            default:
                ESP_LOGE(TAG, "Unknown data type");
                continue;
            }

            if (send_cb.payload == NULL) continue;

            if (xQueueSend(send_queue, &send_cb, ESPNOW_MAXDELAY) == pdTRUE) {
                ESP_LOGI(TAG, "Data read successfully");
            } else {
                ESP_LOGW(TAG, "Send send queue fail");
                free(send_cb.payload);
            }
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
    }
    vTaskDelete(NULL);
}

void RecvProcessingCallback(uint16_t can_id, uint8_t* payload, int payload_len)
{
    switch (can_id)
    {
    case 0x123:{
        float data = *(float *)(payload);
        ESP_LOGI(TAG, "Received float data: %f", data);
        break;
    }
    case 0x456:{
        int32_t int_data = *(int32_t *)(payload);
        ESP_LOGI(TAG, "Received int data: %ld", int_data);
        break;
    }
    case 0x789:{
        uint8_t str_len = *(int8_t *)(payload);
        char *str_data = (char *)(payload + 1);
        str_data[str_len] = '\0'; // Null-terminate the string
        ESP_LOGI(TAG, "Received string data: %s", str_data);
        break;
    }
    default:
        ESP_LOGE(TAG, "Unknown data type");
        PrintPacket(payload, payload_len);
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
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    WiFiInit();
    ESP_LOGI(TAG, "WiFi initialized");
    WCAN_Init();
    ESP_LOGI(TAG, "Setup completed");

    uint8_t mac[6];

    // Read the STA MAC from efuse
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK) {
        // if mac is this: 54:32:04:8c:0b:8c activate read task
        uint8_t target_mac[6] = {0x54, 0x32, 0x04, 0x8c, 0x0b, 0x8c};
        if (memcmp(mac, target_mac, sizeof(mac)) == 0) {
            ESP_LOGI(TAG, "This is a master device");
        } else {
            ESP_LOGI(TAG, "This is a sensor device, starting read task");
            xTaskCreate(ReadDataTask, "ReadDataTask", 4096, NULL, 5, NULL);
        }
    } else {
        printf("Failed to read MAC address\n");
    }
}