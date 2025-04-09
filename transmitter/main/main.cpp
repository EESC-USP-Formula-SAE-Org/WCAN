#include <stdio.h>
#include <nvs_flash.h>
#include <stdbool.h>
#include "string.h"
#include "esp_wifi.h"

#include "espnow_receiver.h"
#include "time_t.h"

static const char *TAG = "MAIN";

//data type enum
typedef enum {
    UINT = 0,
    INT,
    FLOAT
} data_type_t;

//acquisitor struct 
typedef struct {
    uint8_t mac_addr[6];
    size_t can_id;
    data_type_t data_type;
    size_t data_count_per_payload;
    bool is_online;
    uint32_t *data;
} acquisitor_t;

//list of acquisitors
static acquisitor_t acquisitors[] = {
    {{0x24, 0x6F, 0x28, 0xAA, 0xBB, 0xCC}, 0x00, UINT, 0, false, NULL},
    {{0x24, 0x6F, 0x28, 0xDD, 0xEE, 0xFF}, 0x01, UINT, 0, false, NULL},
    {{0x24, 0x6F, 0x28, 0x11, 0x22, 0x33}, 0x02, UINT, 0, false, NULL}
};

#define NUM_ACQUISITORS (sizeof(acquisitors)/sizeof(acquisitor_t))


const size_t frame_time_ms = 1000; //time between frames in ms
//startup function

void GetCommunication(acquisitor_t* acquisitor){
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, acquisitor->mac_addr, 6);
    peerInfo.channel = ESPNOW_CHANNEL;
    peerInfo.encrypt = false; // Set to true if encryption is enabled

    ESP_ERROR_CHECK(esp_now_add_peer(&peerInfo));
    ESP_LOGI(TAG, "Added peer: " MACSTR, MAC2STR(peerInfo.peer_addr));

    time_t current_time = get_rtc_seconds();
    size_t message_size = sizeof(frame_time_ms) + sizeof(current_time) + 2;
    char *message = (char *)malloc(message_size);
    if (message == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for message");
        return;
    }
    snprintf(message, message_size, "%d %lld", frame_time_ms, current_time);

    esp_err_t result = esp_now_send(acquisitor->mac_addr, (uint8_t *)message, strlen(message));
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "ESP-NOW Send failed: %s", esp_err_to_name(result));
    } else {
        ESP_LOGI(TAG, "Connecting to " MACSTR, MAC2STR(acquisitor->mac_addr));
    }
}

static void ReceiveHandshakeCallBack(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{
    uint8_t * mac_addr = recv_info->src_addr;

    if (mac_addr == NULL || data == NULL || len <= 0) {
        ESP_LOGE(TAG, "Receive callback arg error");
        return;
    }

    // Parse received data
    if (len < sizeof(example_espnow_data_t)) {
        ESP_LOGE(TAG, "Received data too short: %d", len);
        return;
    }

    example_espnow_data_t *recv_data = (example_espnow_data_t *)data;

    // Identify sender
    size_t sender_id = -1;
    for (int i = 0; i < NUM_ACQUISITORS; i++) {
        if (memcmp(mac_addr, acquisitors[i].mac_addr, 6) == 0) {
            sender_id = i;
            break;
        }
    }

    if (sender_id == -1) {
        ESP_LOGE(TAG, "Unknown sender: " MACSTR, MAC2STR(mac_addr));
        return;
    }

    ESP_LOGI(TAG, "Received %s from: " MACSTR ", CAN ID: %d, len: %d\n", data, MAC2STR(mac_addr), acquisitors[sender_id].can_id, len);
}

static void ReceiveDataCallback(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{
    uint8_t * mac_addr = recv_info->src_addr;

    if (mac_addr == NULL || data == NULL || len <= 0) {
        ESP_LOGE(TAG, "Receive callback arg error");
        return;
    }


    // Parse received data
    if (len < sizeof(example_espnow_data_t)) {
        ESP_LOGE(TAG, "Received data too short: %d", len);
        return;
    }

    example_espnow_data_t *recv_data = (example_espnow_data_t *)data;

    // Identify sender
    size_t sender_id = -1;
    for (int i = 0; i < NUM_ACQUISITORS; i++) {
        if (memcmp(mac_addr, acquisitors[i].mac_addr, 6) == 0) {
            sender_id = i;
            break;
        }
    }

    if (sender_id == -1) {
        ESP_LOGE(TAG, "Unknown sender: " MACSTR, MAC2STR(mac_addr));
        return;
    }

    ESP_LOGI(TAG, "Received %s from: " MACSTR ", CAN ID: %d, len: %d\n", data, MAC2STR(mac_addr), acquisitors[sender_id].can_id, len);
}

void Setup(){
    ESP_ERROR_CHECK(esp_now_init());
    ESP_LOGI(TAG, "ESP-NOW initialized");

    ESP_ERROR_CHECK(esp_now_register_recv_cb(ReceiveHandshakeCallBack));
    ESP_LOGI(TAG, "ESP-NOW handshake callback registered");

    for(int i = 0; i < NUM_ACQUISITORS; i++){
        GetCommunication(&acquisitors[i]);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    ESP_LOGI(TAG, "Called for all acquisitors");

    size_t max_timeout_ms = 5000;
    //TODO insert here a while loop that blinks a LED or something
    vTaskDelay(pdMS_TO_TICKS(max_timeout_ms));
    ESP_ERROR_CHECK(esp_now_unregister_recv_cb());
    ESP_LOGI(TAG, "Timeout reached, removing handshake callback");


    ESP_ERROR_CHECK(esp_now_register_recv_cb(ReceiveDataCallback));
    ESP_LOGI(TAG, "ESP-NOW data callback registered");
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
    Setup();
}