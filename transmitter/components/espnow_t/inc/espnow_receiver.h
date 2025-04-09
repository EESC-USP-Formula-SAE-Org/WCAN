#ifndef ESPNOW_RECEIVER_H
#define ESPNOW_RECEIVER_H
#include <stdint.h>
#include <stdbool.h>
#include "esp_now.h"
#include "esp_log.h"

#if CONFIG_ESPNOW_WIFI_MODE_STATION
#define ESPNOW_WIFI_MODE WIFI_MODE_STA
#define WIFI_IF WIFI_IF_STA
#define ESPNOW_WIFI_IF   ESP_IF_WIFI_STA
#else
#define ESPNOW_WIFI_MODE WIFI_MODE_AP
#define WIFI_IF WIFI_IF_AP
#define ESPNOW_WIFI_IF   ESP_IF_WIFI_AP
#endif

#ifndef MAC2STR
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#endif

#define ESPNOW_CHANNEL 1

// Structure of received ESPNOW data
typedef struct {
    uint8_t type;
    uint8_t state;
    uint16_t seq_num;
    uint32_t magic;
    uint8_t payload[250];
    uint16_t crc;
} __attribute__((packed)) example_espnow_data_t;

#endif // ESPNOW_RECEIVER_H

