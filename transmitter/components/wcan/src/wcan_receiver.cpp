#include "string.h"
#include "esp_err.h"
#include "esp_now.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "wcan_receiver.h"
#include "wcan_utils.h"
#include "wcan_sender.h"

QueueHandle_t recv_queue = NULL;

void AckSendTask(void *pvParameter);

void RecvProcessingTask(void *pvParameter){
    static const char *TAG = "RECV";
    recv_queue = xQueueCreate(RECV_QUEUE_SIZE, sizeof(data_packet_t));
    if (recv_queue == NULL) {
        ESP_LOGE(TAG, "Create receive queue fail");
        return;
    }
    ESP_LOGI(TAG, "Receive queue created");

    data_packet_t recv_data_packet;
    while (1) {
        if (xQueueReceive(recv_queue, &recv_data_packet, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "Processing received data with id: %04x", recv_data_packet.can_id);
            xTaskCreate(AckSendTask, "AckSendTask", 2048, &recv_data_packet, 5, NULL);
            if (RecvCallback) {
                RecvCallback(recv_data_packet);
            } else {
                ESP_LOGW(TAG, "No callback function defined for received data");
                break;
            }
        }
    }
    vTaskDelete(NULL);
}

void AckSendTask(void *pvParameter){
    static const char *TAG = "ACK";
    data_packet_t *recv_packet = (data_packet_t *)pvParameter;
    //acknowledge to MAC that received can_id
    char *ack_mac = MacToString(recv_packet->mac_addr);
    ESP_LOGI(TAG, "Acknowledging that received ID: %04X from (%s)", 
                recv_packet->can_id, ack_mac);
    free(ack_mac);

    data_packet_t ack_data;
    memcpy(ack_data.mac_addr, recv_packet->mac_addr, ESP_NOW_ETH_ALEN);
    ack_data.can_id = ACK_ID;
    ack_data.payload = (uint8_t *)malloc(sizeof(uint16_t));
    if (ack_data.payload == NULL) {
        ESP_LOGE(TAG, "Malloc ack payload fail");
        return;
    }
    memcpy(ack_data.payload, &recv_packet->can_id, sizeof(uint16_t));
    ack_data.payload_len = sizeof(uint16_t);

    AddPeer(ack_data.mac_addr);
    SendData(ack_data.mac_addr, ack_data);
    RemovePeer(ack_data.mac_addr);
    vTaskDelete(NULL);
}

void FilterData(data_packet_t data)
{
    static const char *TAG = "FILTER";

    ESP_LOGI(TAG, "Received data with id: %04x", data.can_id);
    if (recv_filter) {
        bool found = false;
        for (int i = 0; i < recv_allowed_ids_size; i++) {
            if (data.can_id == recv_allowed_ids[i]) {
                found = true;
                break;
            }
        }
        if (!found) {
            ESP_LOGI(TAG, "Filtered data with id: %04x", data.can_id);
            free(data.payload);
            return;
        }
    }

    if (xQueueSend(recv_queue, &data, ESPNOW_MAXDELAY) != pdTRUE) {
        ESP_LOGW(TAG, "Send receive queue fail");
        free(data.payload);
    }
}
