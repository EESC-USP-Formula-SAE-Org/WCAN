#include <stdio.h>
#include <stdbool.h>

#include "string.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "wcan_communication.h"
#include "wcan_utils.h"
#include "wcan_sender.h"
#include "wcan_receiver.h"

const uint16_t ACK_ID = 0xF800;
bool recv_filter = false;
uint16_t *recv_allowed_ids = NULL;
size_t recv_allowed_ids_size = 0;

static void ESPNOW_SendCallback(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    static const char *TAG = "SEND";
    if (mac_addr == NULL) {
        ESP_LOGE(TAG, "Send cb arg error");
        return;
    }

    if (status == ESP_NOW_SEND_FAIL) {
        ESP_LOGE(TAG, "Failed");
    }else{
        ESP_LOGI(TAG, "Success");
    }
}

static void ESPNOW_RecvCallback(const esp_now_recv_info_t *recv_info, const uint8_t *data, int data_len)
{
    static const char *TAG = "RECV";
    esp_now_packet_t *recv_packet = (esp_now_packet_t *)malloc(sizeof(esp_now_packet_t));
    if (recv_packet == NULL) {
        ESP_LOGE(TAG, "Malloc receive cb fail");
        return;
    }

    uint8_t * mac_addr = recv_info->src_addr;
    if (mac_addr == NULL || data == NULL || data_len <= 0) {
        ESP_LOGE(TAG, "Receive cb arg error");
        return;
    }
    memcpy(recv_packet->mac_addr, mac_addr, ESP_NOW_ETH_ALEN);

    recv_packet->data = (uint8_t*)malloc(data_len);
    if (recv_packet->data == NULL) {
        ESP_LOGE(TAG, "Malloc receive data fail");
        free(recv_packet);
        return;
    }
    memcpy(recv_packet->data, data, data_len);
    recv_packet->data_len = data_len;

    ESP_LOGI(TAG, "Received payload of size %d from %02x:%02x:%02x:%02x:%02x:%02x", 
                data_len, mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    PrintCharPacket(recv_packet->data, data_len);

    data_packet_t recv_data = DecodeDataPacket(*recv_packet);
    FreeESPNOWPacket(recv_packet);

    if (recv_data.can_id == ACK_ID) {
        AckRecv();
    }else{
        FilterData(recv_data);
    }
}

void WCAN_Init(bool filter, uint16_t *allowed_ids, size_t allowed_ids_size){
    static const char *TAG = "WCAN";
    ESP_ERROR_CHECK(esp_now_init());
    ESP_LOGD(TAG, "ESP-NOW initialized");
    AddPeer(BROADCAST_MAC);
    ESP_LOGD(TAG, "Broadcast peer added");
    ESP_ERROR_CHECK(esp_now_register_send_cb(ESPNOW_SendCallback));
    ESP_LOGD(TAG, "ESP-NOW send callback registered");
    ESP_ERROR_CHECK(esp_now_register_recv_cb(ESPNOW_RecvCallback));
    ESP_LOGD(TAG, "ESP-NOW receive callback registered");
    
    recv_filter = filter;
    if (filter) {
        recv_allowed_ids = allowed_ids;
        recv_allowed_ids_size = allowed_ids_size;
    }else{
        recv_allowed_ids = NULL;
        recv_allowed_ids_size = 0;
    }

    xTaskCreate(SendProcessingTask, "SendProcessingTask", 4096, NULL, 5, NULL);
    xTaskCreate(RecvProcessingTask, "RecvProcessingTask", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "WCAN initialized");
}
