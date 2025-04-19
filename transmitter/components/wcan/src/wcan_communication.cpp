#include "wcan_communication.h"

QueueHandle_t send_queue = NULL;
QueueHandle_t recv_queue = NULL;

void PrintPacket(uint8_t *data, int data_len)
{
    printf("Data: ");
    for (int i = 0; i < data_len; i++) {
        printf("%02x ", data[i]);
    }
    printf("\n");
}

static void SendData(send_packet_t send_cb){
    static const char *TAG = "SEND";
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
    PrintPacket(data, payload_len + 2);

    esp_err_t ret = esp_now_send(broadcast_mac, data, payload_len + 2);
    free(data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Send data failed: %s", esp_err_to_name(ret));
    }
}

static void IRAM_ATTR ResendData(TimerHandle_t xTimer) {

    if (retry_count < WCAN_RETRY_COUNT) {
        ESP_LOGI("Resend", "Timeout reached, resending %04x... Attempt: %d of %d", cur_send_packet->can_id, retry_count + 1, WCAN_RETRY_COUNT);
        SendData(*cur_send_packet);
        retry_count++;
    } else {
        ESP_LOGE("Resend", "Max retry attempts reached");

        if(uxSemaphoreGetCount(send_mutex) == 0) {
            xSemaphoreGive(send_mutex);
        }

        if (cur_send_packet->payload != NULL) {
            free(cur_send_packet->payload);
            cur_send_packet->payload = NULL;
        }

        if (cur_send_packet != NULL) {
            free(cur_send_packet);
            cur_send_packet = NULL;
        }
        
        if(resend_timer != NULL) {
            xTimerStop(resend_timer, 0);
            xTimerDelete(resend_timer, 0);
        }
    }
}

static void SendProcessingTask(void *pvParameter)
{
    static const char *TAG = "SEND";

    send_queue = xQueueCreate(10, sizeof(send_packet_t));
    if (send_queue == NULL) {
        ESP_LOGE(TAG, "Create send queue fail");
        return;
    }
    ESP_LOGI(TAG, "Send queue created");

    send_mutex = xSemaphoreCreateBinary();
    if (send_mutex == NULL) {
        ESP_LOGE(TAG, "Create send mutex fail");
        return;
    }
    xSemaphoreGive(send_mutex);

    send_packet_t send_cb;
    while (xQueueReceive(send_queue, &send_cb, portMAX_DELAY) == pdTRUE) {
        ESP_LOGI(TAG, "Sending data with id: %04x", send_cb.can_id);

        xSemaphoreTake(send_mutex, portMAX_DELAY); //! CRITICAL ZONE

        cur_send_packet = (send_packet_t*)malloc(sizeof(send_packet_t));
        if (cur_send_packet == NULL) {
            ESP_LOGE(TAG, "Malloc for current send packet fail");
            free(send_cb.payload);
            return;
        }
        memcpy(cur_send_packet, &send_cb, sizeof(send_packet_t));
        
        ESP_LOGI(TAG, "Send mutex taken (%d)", uxSemaphoreGetCount(send_mutex));
        SendData(*cur_send_packet);
        retry_count = 0;
        //copy resend data
        resend_timer = xTimerCreate("ResendTimer", pdMS_TO_TICKS(ESPNOW_MAXDELAY), pdTRUE, NULL, ResendData);
        if (resend_timer == NULL) {
            ESP_LOGE(TAG, "Create resend timer fail");
            free(send_cb.payload);
            return;
        }
        xTimerStart(resend_timer, 0);
    
    }
}

static void AddPeer(uint8_t *mac_addr)
{
    static const char *TAG = "PEER";
    esp_now_peer_info_t *peer = (esp_now_peer_info_t *)malloc(sizeof(esp_now_peer_info_t));
    if (peer == NULL) {
        ESP_LOGE(TAG, "Malloc peer information fail");
        return;
    }
    memset(peer, 0, sizeof(esp_now_peer_info_t));
    peer->channel = ESPNOW_CHANNEL;
    peer->ifidx = ESPNOW_WIFI_IF;
    peer->encrypt = false;
    memcpy(peer->peer_addr, mac_addr, ESP_NOW_ETH_ALEN);
    ESP_ERROR_CHECK(esp_now_add_peer(peer));
    ESP_LOGI(TAG, "Peer added: %02x:%02x:%02x:%02x:%02x:%02x", 
            mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    free(peer);
}

static void RemovePeer(uint8_t *mac_addr)
{
    static const char *TAG = "PEER";
    esp_err_t ret = esp_now_del_peer(mac_addr);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Remove peer failed: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "Peer removed");
}

static void AckSend(recv_packet_t recv_cb)
{
    static const char *TAG = "ACK";
    //add sender mac address to peer list
    AddPeer(recv_cb.mac_addr);
    uint8_t *ack_data = (uint8_t *)malloc(4);
    if (ack_data == NULL) {
        ESP_LOGE(TAG, "Malloc ack data fail");
        return;
    }
    memcpy(ack_data, &ack_id, sizeof(ack_id));
    memcpy(ack_data + 2, recv_cb.data, sizeof(uint16_t));

    ESP_ERROR_CHECK(esp_now_send(recv_cb.mac_addr, ack_data, 4));
    free(ack_data);
    ESP_LOGI(TAG, "Acknowledgment sent");
    RemovePeer(recv_cb.mac_addr);
}

static void AckRecv(recv_packet_t *recv_cb)
{
    static const char *TAG = "ACK";

    ESP_LOGI(TAG, "Received acknowledgment");
    if (uxSemaphoreGetCount(send_mutex) == 0) {
        BaseType_t woken = pdFALSE;
        xSemaphoreGiveFromISR(send_mutex, &woken);
        portYIELD_FROM_ISR(woken);
    }

    if (cur_send_packet->payload != NULL) {
        free(cur_send_packet->payload);
        cur_send_packet->payload = NULL;
    }

    if (cur_send_packet != NULL) {
        free(cur_send_packet);
        cur_send_packet = NULL;
    }
    
    xTimerStop(resend_timer, 0);
    xTimerDelete(resend_timer, 0);

    free(recv_cb->data);
    free(recv_cb);
}

static void RecvProcessingTask(void *pvParameter)
{
    static const char *TAG = "RECV";
    recv_queue = xQueueCreate(10, sizeof(recv_packet_t));
    if (recv_queue == NULL) {
        ESP_LOGE(TAG, "Create receive queue fail");
        return;
    }
    ESP_LOGI(TAG, "Receive queue created");

    recv_packet_t recv_cb;

    while (xQueueReceive(recv_queue, &recv_cb, portMAX_DELAY) == pdTRUE) {
        
        uint16_t can_id;
        memcpy(&can_id, recv_cb.data, sizeof(uint16_t));
        ESP_LOGI(TAG, "Processing received data with id: %04x", can_id);
        PrintPacket(recv_cb.data, recv_cb.data_len);

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

        AckSend(recv_cb);
        free(recv_cb.data);

        if(!RecvProcessingCallback){
            vTaskDelete(NULL);
            return;
        }

        RecvProcessingCallback(can_id, payload, len);
        free(payload);
    }
    vTaskDelete(NULL);
}

static void ESPNOW_SendCallback(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    static const char *TAG = "SEND";
    if (mac_addr == NULL) {
        ESP_LOGE(TAG, "Send cb arg error");
        return;
    }
}

static void ESPNOW_RecvCallback(const esp_now_recv_info_t *recv_info, const uint8_t *data, int data_len)
{
    static const char *TAG = "RECV";
    recv_packet_t *recv_cb = (recv_packet_t *)malloc(sizeof(recv_packet_t));
    if (recv_cb == NULL) {
        ESP_LOGE(TAG, "Malloc receive cb fail");
        return;
    }

    uint8_t * mac_addr = recv_info->src_addr;
    if (mac_addr == NULL || data == NULL || data_len <= 0) {
        ESP_LOGE(TAG, "Receive cb arg error");
        return;
    }
    
    memcpy(recv_cb->mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
    recv_cb->data = (uint8_t*)malloc(data_len);
    if (recv_cb->data == NULL) {
        ESP_LOGE(TAG, "Malloc receive data fail");
        free(recv_cb);
        return;
    }
    memcpy(recv_cb->data, data, data_len);
    recv_cb->data_len = data_len;
    ESP_LOGI(TAG, "Received payload of size %d from %02x:%02x:%02x:%02x:%02x:%02x", 
                data_len, mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    PrintPacket(recv_cb->data, data_len);

    uint16_t can_id;
    memcpy(&can_id, recv_cb->data, sizeof(uint16_t));

    if (can_id == ack_id) {
        AckRecv(recv_cb);
        return;
    }

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
            free(recv_cb);
            return;
        }
    }

    if (xQueueSend(recv_queue, recv_cb, ESPNOW_MAXDELAY) != pdTRUE) {
        ESP_LOGW(TAG, "Send receive queue fail");
        free(recv_cb->data);
        free(recv_cb);
    }
}

void WCAN_Init(){
    static const char *TAG = "WCAN";
    ESP_ERROR_CHECK(esp_now_init());
    ESP_LOGI(TAG, "ESP-NOW initialized");
    AddPeer(broadcast_mac);
    ESP_LOGI(TAG, "Broadcast peer added");
    ESP_ERROR_CHECK(esp_now_register_send_cb(ESPNOW_SendCallback));
    ESP_LOGI(TAG, "ESP-NOW send callback registered");
    ESP_ERROR_CHECK(esp_now_register_recv_cb(ESPNOW_RecvCallback));
    ESP_LOGI(TAG, "ESP-NOW receive callback registered");

    xTaskCreate(SendProcessingTask, "SendProcessingTask", 4096, NULL, 4, NULL);
    xTaskCreate(RecvProcessingTask, "RecvProcessingTask", 4096, NULL, 3, NULL);
}
