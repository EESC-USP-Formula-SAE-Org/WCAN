#include "wcan_communication.h"

static const char *TAG = "WCAN";

QueueHandle_t send_queue = NULL;
QueueHandle_t recv_queue = NULL;

static void SendProcessingTask(void *pvParameter)
{
    send_queue = xQueueCreate(10, sizeof(event_send_cb_t));
    if (send_queue == NULL) {
        ESP_LOGE(TAG, "Create send queue fail");
        return;
    }
    ESP_LOGI(TAG, "Send queue created");

    event_send_cb_t send_cb;

    while (xQueueReceive(send_queue, &send_cb, portMAX_DELAY) == pdTRUE) {
        uint8_t *payload = send_cb.payload;
        int payload_len = send_cb.payload_len;
        uint16_t can_id = send_cb.can_id;
        uint8_t *data = (uint8_t *)malloc(2 + payload_len);
        if (data == NULL) {
            ESP_LOGE(TAG, "Malloc send data fail");
            return;
        }
        memcpy(data, &can_id, sizeof(can_id));
        memcpy(data + 2, payload, payload_len);
        ESP_LOGI(TAG, "CAN_ID: %04x", can_id);
        printf("Sending data: ");
        for (int i = 0; i < 2 + payload_len; i++) {
            printf("%02x ", data[i]);
        }
        printf("\n");
        free(send_cb.payload);
        send_cb.payload = NULL;

        esp_err_t ret = esp_now_send(broadcast_mac, data, payload_len + 2);
        free(data);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Send data failed: %s", esp_err_to_name(ret));
        }
    }
}

static void RecvProcessingTask(void *pvParameter)
{
    recv_queue = xQueueCreate(10, sizeof(event_recv_cb_t));
    if (recv_queue == NULL) {
        ESP_LOGE(TAG, "Create receive queue fail");
        return;
    }
    ESP_LOGI(TAG, "Receive queue created");

    event_recv_cb_t recv_cb;

    while (xQueueReceive(recv_queue, &recv_cb, portMAX_DELAY) == pdTRUE) {
        printf("Data to be interpreted: ");
        for (int i = 0; i < recv_cb.data_len; i++) {
            printf("%02x ", recv_cb.data[i]);
        }
        printf("\n");
        
        uint16_t can_id;
        memcpy(&can_id, recv_cb.data, sizeof(uint16_t));
        ESP_LOGI(TAG, "Received data with id: %04x", can_id);
        size_t len = recv_cb.data_len - 2;
        if (len <= 0) {
            ESP_LOGE(TAG, "Received data length is invalid");
            free(recv_cb.data);
            return;
        }
        uint8_t *payload = (uint8_t *)malloc(len);
        if (payload == NULL) {
            ESP_LOGE(TAG, "Malloc payload fail");
            free(recv_cb.data);
            return;
        }
        memcpy(payload, recv_cb.data + 2, len);

        if(!RecvProcessing){
            vTaskDelete(NULL);
            return;
        }

        RecvProcessing(can_id, payload, len);
        free(payload);
    }
    vTaskDelete(NULL);
}

static void SendCallback(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    if (mac_addr == NULL) {
        ESP_LOGE(TAG, "Send cb arg error");
        return;
    }
}

static void ReceiveCallback(const esp_now_recv_info_t *recv_info, const uint8_t *data, int data_len)
{
    event_recv_cb_t *recv_cb = (event_recv_cb_t *)malloc(sizeof(event_recv_cb_t));

    uint8_t * mac_addr = recv_info->src_addr;
    if (mac_addr == NULL || data == NULL || data_len <= 0) {
        ESP_LOGE(TAG, "Receive cb arg error");
        return;
    }
    
    memcpy(recv_cb->mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
    recv_cb->data = (uint8_t*)malloc(data_len);
    if (recv_cb->data == NULL) {
        ESP_LOGE(TAG, "Malloc receive data fail");
        return;
    }
    memcpy(recv_cb->data, data, data_len);
    recv_cb->data_len = data_len;
    ESP_LOGI(TAG, "%02x:%02x:%02x:%02x:%02x:%02x send payload of size: %d", 
            mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5], data_len);
    printf("Received data: ");
    for (int i = 0; i < recv_cb->data_len; i++) {
        printf("%02x ", recv_cb->data[i]);
    }
    printf("\n");

    uint16_t can_id;
    memcpy(&can_id, recv_cb->data, sizeof(uint16_t));
    ESP_LOGI(TAG, "Received data with id: %04x", can_id);
    if (recv_filter) {
        bool found = false;
        for (int i = 0; i < ALLOWED_IDS_SIZE; i++) {
            if (can_id == recv_allowed_ids[i]) {
                found = true;
                break;
            }
        }
        if (!found) {
            ESP_LOGI(TAG, "Filtered data with id: %04x", can_id);
            free(recv_cb->data);
            return;
        }
    }

    if (xQueueSend(recv_queue, recv_cb, ESPNOW_MAXDELAY) != pdTRUE) {
        ESP_LOGW(TAG, "Send receive queue fail");
        free(recv_cb->data);
        free(recv_cb);
    }
}

void Setup(){
    ESP_ERROR_CHECK(esp_now_init());
    ESP_LOGI(TAG, "ESP-NOW initialized");
    esp_now_peer_info_t *peer = (esp_now_peer_info_t *)malloc(sizeof(esp_now_peer_info_t));
    if (peer == NULL) {
        ESP_LOGE(TAG, "Malloc peer information fail");
        esp_now_deinit();
        return;
    }
    memset(peer, 0, sizeof(esp_now_peer_info_t));
    peer->channel = ESPNOW_CHANNEL;
    peer->ifidx = ESPNOW_WIFI_IF;
    peer->encrypt = false;
    memcpy(peer->peer_addr, broadcast_mac, ESP_NOW_ETH_ALEN);
    ESP_ERROR_CHECK(esp_now_add_peer(peer));
    free(peer);

    ESP_ERROR_CHECK(esp_now_register_send_cb(SendCallback));
    ESP_LOGI(TAG, "ESP-NOW send callback registered");
    ESP_ERROR_CHECK(esp_now_register_recv_cb(ReceiveCallback));
    ESP_LOGI(TAG, "ESP-NOW receive callback registered");

    xTaskCreate(SendProcessingTask, "SendProcessingTask", 4096, NULL, 4, NULL);
    xTaskCreate(RecvProcessingTask, "RecvProcessingTask", 4096, NULL, 3, NULL);
}
