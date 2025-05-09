#include <string.h>
#include <stdio.h>

#include "esp_log.h"

#include "wcan_utils.h"

void AddPeer(const uint8_t *mac_addr)
{
    static const char *TAG = "PEER";
    esp_now_peer_info_t *peer = (esp_now_peer_info_t *)malloc(sizeof(esp_now_peer_info_t));
    ESP_LOGV(TAG, "peer: %p\n", (void*)peer);
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
    ESP_LOGV(TAG, "Peer added: %02x:%02x:%02x:%02x:%02x:%02x", 
            mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    free(peer);
}

void RemovePeer(const uint8_t *mac_addr)
{
    static const char *TAG = "PEER";
    esp_err_t ret = esp_now_del_peer(mac_addr);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Remove peer failed: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGV(TAG, "Peer removed");
}

esp_now_packet_t *EncodeDataPacket(const data_packet_t *data_packet){
    static const char *TAG = "ENCODE";
    esp_now_packet_t *esp_now_packet = (esp_now_packet_t *)malloc(sizeof(esp_now_packet_t));
    ESP_LOGV(TAG, "esp_now_packet: %p\n", (void*)esp_now_packet);
    if (esp_now_packet == NULL) {
        ESP_LOGE(TAG, "Malloc esp now packet fail");
        return NULL;
    } 
    esp_now_packet->data_len = sizeof(data_packet->can_id) + data_packet->payload_len;
    esp_now_packet->data = (uint8_t *)malloc(esp_now_packet->data_len);
    ESP_LOGV(TAG, "esp_now_packet->data: %p\n", (void*)esp_now_packet->data);
    if (esp_now_packet->data == NULL) {
        ESP_LOGE(TAG, "Malloc esp now packet fail");
        free(esp_now_packet);
        esp_now_packet = NULL;
        return NULL;
    }
    size_t can_id_len = sizeof(data_packet->can_id);
    memcpy(esp_now_packet->data, &data_packet->can_id, can_id_len);
    memcpy(esp_now_packet->data + can_id_len, data_packet->payload, data_packet->payload_len);
    return esp_now_packet;
}

data_packet_t *DecodeDataPacket(const esp_now_packet_t *esp_now_packet){
    static const char *TAG = "DECODE";
    data_packet_t *data_packet = (data_packet_t *)malloc(sizeof(data_packet_t));
    ESP_LOGV(TAG, "data_packet: %p\n", (void*)data_packet);
    if (data_packet == NULL) {
        ESP_LOGE(TAG, "Malloc data packet fail");
        return NULL;
    }
    memcpy(data_packet->mac_addr, esp_now_packet->mac_addr, ESP_NOW_ETH_ALEN);
    data_packet->can_id = *(uint16_t *)(esp_now_packet->data);
    data_packet->payload_len = esp_now_packet->data_len - sizeof(data_packet->can_id);
    data_packet->payload = (uint8_t *)malloc(data_packet->payload_len);
    ESP_LOGV(TAG, "data_packet->payload: %p\n", (void*)data_packet->payload);
    if (data_packet->payload == NULL) {
        ESP_LOGE(TAG, "Malloc payload fail");
        free(data_packet);
        data_packet = NULL;
        return NULL;
    }
    memcpy(data_packet->payload, esp_now_packet->data + sizeof(data_packet->can_id), data_packet->payload_len);
    return data_packet;
}

void PrintCharPacket(const uint8_t *data, const int data_len){
    // create a string buffer to hold the formatted string
    static const char *TAG = "DATA";
    size_t buf_len = data_len * 3 + 1;
    char *str = (char*)malloc(buf_len);
    ESP_LOGV(TAG, "str: %p\n", (void*)str);
    if (!str) return;
    char *p = str;
    for (size_t i = 0; i < data_len; i++) {
        int written = snprintf(p, 4, "%02x ", data[i]);
        p += written;
    }
    *p = '\0';
    // print the formatted string
    ESP_LOGV(TAG, "%s", str);
    free(str);
}
