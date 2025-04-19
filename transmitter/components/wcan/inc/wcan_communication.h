#ifndef __WCAN_COMMUNICATION_H__
#define __WCAN_COMMUNICATION_H__

#include <stdio.h>
#include <stdbool.h>

#include "string.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#if CONFIG_ESPNOW_WIFI_MODE_STATION
#define ESPNOW_WIFI_MODE WIFI_MODE_STA
#define ESPNOW_WIFI_IF   WIFI_IF_STA
#else
#define ESPNOW_WIFI_MODE WIFI_MODE_AP
#define ESPNOW_WIFI_IF   WIFI_IF_AP
#endif

#define ESPNOW_CHANNEL 1
#define ESPNOW_MAXDELAY 500
#define WCAN_RETRY_COUNT 5


typedef struct {
    uint16_t can_id;
    uint8_t *payload;
    int payload_len;
} send_packet_t;
extern QueueHandle_t send_queue;
static uint8_t broadcast_mac[ESP_NOW_ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
static uint16_t ack_id = 0xF800;
static SemaphoreHandle_t send_mutex;
static send_packet_t *cur_send_packet;
static size_t retry_count = 0;
static TimerHandle_t resend_timer;

typedef struct {
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];
    uint8_t *data;
    int data_len;
} recv_packet_t;
extern QueueHandle_t recv_queue;
static bool recv_filter = true;
static uint16_t recv_allowed_ids[3] = {0x123, 0x456, 0x789};
#define ALLOWED_IDS_SIZE (sizeof(recv_allowed_ids) / sizeof(recv_allowed_ids[0]))

void WCAN_Init();
void RecvProcessingCallback(uint16_t can_id, uint8_t* payload, int payload_len) __attribute__((weak));
void PrintPacket(uint8_t *data, int data_len);

#endif
