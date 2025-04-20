#include "string.h"
#include "esp_err.h"
#include "esp_now.h"
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "wcan_sender.h"
#include "wcan_communication.h"
#include "wcan_utils.h"

const uint8_t BROADCAST_MAC[ESP_NOW_ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
QueueHandle_t send_queue = NULL;
SemaphoreHandle_t send_semaphore = NULL;
resend_t resend_ctx;

void StartResendScheduler();
void StopResendScheduler();
void ResendData(TimerHandle_t xTimer);

void SendProcessingTask(void *pvParameter)
{
    static const char *TAG = "SEND";
    send_queue = xQueueCreate(SEND_QUEUE_SIZE, sizeof(data_packet_t));
    if (send_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create send queue");
        vTaskDelete(NULL);
    }

    send_semaphore = xSemaphoreCreateBinary();
    if (send_semaphore == NULL) {
        ESP_LOGE(TAG, "Failed to create send semaphore");
        vTaskDelete(NULL);
    }
    xSemaphoreGive(send_semaphore);

    ESP_LOGI(TAG, "Send processing task started");

    data_packet_t send_data_packet;
    while (1) {
        if (xQueueReceive(send_queue, &send_data_packet, portMAX_DELAY) == pdTRUE) {
            xSemaphoreTake(send_semaphore, portMAX_DELAY); //! CRITICAL ZONE
            ESP_LOGD(TAG, "Processing data with id: %04x", send_data_packet.can_id);

            resend_ctx.data_packet = (data_packet_t*)malloc(sizeof(data_packet_t));
            if (resend_ctx.data_packet == NULL) {
                ESP_LOGE(TAG, "Malloc for current send packet fail");
                free(send_data_packet.payload);
                break;
            }
            memcpy(resend_ctx.data_packet, &send_data_packet, sizeof(data_packet_t));
            
            SendData(BROADCAST_MAC, *resend_ctx.data_packet);
            
            StartResendScheduler();
        }
    }
    vTaskDelete(NULL);
}

void SendData(const uint8_t* mac_addr, const data_packet_t data_packet){
    static const char *TAG = "SEND";

    esp_now_packet_t esp_now_packet = EncodeDataPacket(data_packet);
    memcpy(esp_now_packet.mac_addr, mac_addr, ESP_NOW_ETH_ALEN);

    PrintCharPacket(esp_now_packet.data, esp_now_packet.data_len);

    ESP_ERROR_CHECK(esp_now_send(esp_now_packet.mac_addr, esp_now_packet.data, esp_now_packet.data_len));
    char *send_mac = MacToString(esp_now_packet.mac_addr);
    ESP_LOGI(TAG, "Data sent to %s", send_mac);
    free(send_mac);
}

void StartResendScheduler(){
    static const char *TAG = "RESEND";

    resend_ctx.retry_count = 0;
    resend_ctx.timer = xTimerCreate("ResendTimer", pdMS_TO_TICKS(ESPNOW_MAXDELAY), pdTRUE, NULL, ResendData);
    if (resend_ctx.timer == NULL) {
        ESP_LOGE(TAG, "Create resend timer fail");
        free(resend_ctx.data_packet->payload);
        return;
    }

    xTimerStart(resend_ctx.timer, 0);
    ESP_LOGD(TAG, "Resend timer started");
}

void StopResendScheduler()
{
    static const char *TAG = "RESEND";

    ESP_LOGD(TAG, "Stopping resend timer (%d)", uxSemaphoreGetCount(send_semaphore));

    FreeDataPacket(resend_ctx.data_packet);
    
    if(resend_ctx.timer != NULL) {
        xTimerStop(resend_ctx.timer, 0);
        xTimerDelete(resend_ctx.timer, 0);
        resend_ctx.timer = NULL;
        ESP_LOGV(TAG, "Resend timer deleted");
    }

    if(uxSemaphoreGetCount(send_semaphore) == 0) {
        xSemaphoreGive(send_semaphore);
        ESP_LOGV(TAG, "Send mutex released");
    }
}

void ResendData(TimerHandle_t xTimer) {
    static const char *TAG = "RESEND";

    if (resend_ctx.retry_count < WCAN_MAX_RETRY_COUNT) {
        ESP_LOGW(TAG, "Timeout reached, resending %04x... Attempt: %d of %d", 
                    resend_ctx.data_packet->can_id, resend_ctx.retry_count + 1, WCAN_MAX_RETRY_COUNT);

        SendData(BROADCAST_MAC, *resend_ctx.data_packet);
        resend_ctx.retry_count++;
    } else {
        ESP_LOGE(TAG, "Max retry attempts reached");
        StopResendScheduler();
    }
}

void AckRecv()
{
    static const char *TAG = "ACK";
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
    ESP_LOGD(TAG, "Acknowledged data received");
    StopResendScheduler();
}
