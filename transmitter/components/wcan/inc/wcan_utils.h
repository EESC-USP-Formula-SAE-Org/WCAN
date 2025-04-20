#ifndef __WCAN_UTILS_H__
#define __WCAN_UTILS_H__

#include "wcan_communication.h"

esp_now_packet_t *EncodeDataPacket(const data_packet_t *data_packet);
void FreeESPNOWPacket(esp_now_packet_t *esp_now_packet);

data_packet_t *DecodeDataPacket(const esp_now_packet_t *esp_now_packet);
void FreeDataPacket(data_packet_t *data_packet);

void PrintCharPacket(const uint8_t *data, const int data_len);
char *MacToString(const uint8_t *mac_addr);

void AddPeer(const uint8_t *mac_addr);
void RemovePeer(const uint8_t *mac_addr);

#endif // __WCAN_UTILS_H__