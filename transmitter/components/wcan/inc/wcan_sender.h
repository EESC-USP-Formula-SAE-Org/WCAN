#ifndef __WCAN_SENDER_H__
#define __WCAN_SENDER_H__

#include "wcan_communication.h"

extern const uint8_t BROADCAST_MAC[ESP_NOW_ETH_ALEN];

#define WCAN_MAX_RETRY_COUNT 3
#define WCAN_RETRY_DELAY 200
typedef struct {
    data_packet_t *data_packet;
    size_t retry_count;
    TimerHandle_t timer;
} resend_t;

void SendProcessingTask(void *pvParameter);
void AckRecv();
void SendData(const uint8_t* mac_addr, const data_packet_t data_packet);

#endif