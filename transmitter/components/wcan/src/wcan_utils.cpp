#include <string.h>
#include <stdio.h>

#include "esp_log.h"

#include "wcan_utils.h"

void AddPeer(const uint8_t *mac_addr)
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
    ESP_LOGD(TAG, "Peer added: %02x:%02x:%02x:%02x:%02x:%02x", 
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
    ESP_LOGD(TAG, "Peer removed");
}

esp_now_packet_t *EncodeDataPacket(const data_packet_t *data_packet){
    static const char *TAG = "ENCODE";
    esp_now_packet_t *esp_now_packet = (esp_now_packet_t *)malloc(sizeof(esp_now_packet_t));
    if (esp_now_packet == NULL) {
        ESP_LOGE(TAG, "Malloc esp now packet fail");
        return NULL;
    } 
    esp_now_packet->data_len = sizeof(data_packet->can_id) + data_packet->payload_len;
    esp_now_packet->data = (uint8_t *)malloc(esp_now_packet->data_len);
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
void FreeESPNOWPacket(esp_now_packet_t *esp_now_packet){
    static const char *TAG = "ENCODE";
    if (esp_now_packet == NULL) {
        ESP_LOGE(TAG, "Free esp now packet arg error");
        return;
    }
    if (esp_now_packet->data != NULL) {
        free(esp_now_packet->data);
        esp_now_packet->data = NULL;
    }
    if (esp_now_packet != NULL) {
        free(esp_now_packet);
        esp_now_packet = NULL;
    }
}

data_packet_t *DecodeDataPacket(const esp_now_packet_t *esp_now_packet){
    static const char *TAG = "DECODE";
    data_packet_t *data_packet = (data_packet_t *)malloc(sizeof(data_packet_t));
    if (data_packet == NULL) {
        ESP_LOGE(TAG, "Malloc data packet fail");
        return NULL;
    }
    memcpy(data_packet->mac_addr, esp_now_packet->mac_addr, ESP_NOW_ETH_ALEN);
    data_packet->can_id = *(uint16_t *)(esp_now_packet->data);
    data_packet->payload_len = esp_now_packet->data_len - sizeof(data_packet->can_id);
    data_packet->payload = (uint8_t *)malloc(data_packet->payload_len);
    if (data_packet->payload == NULL) {
        ESP_LOGE(TAG, "Malloc payload fail");
        free(data_packet);
        data_packet = NULL;
        return NULL;
    }
    memcpy(data_packet->payload, esp_now_packet->data + sizeof(data_packet->can_id), data_packet->payload_len);
    return data_packet;
}

void FreeDataPacket(data_packet_t *data_packet){
    static const char *TAG = "DECODE";
    if (data_packet == NULL) {
        ESP_LOGE(TAG, "Free data packet arg error");
        return;
    }
    if (data_packet->payload != NULL) {
        free(data_packet->payload);
        data_packet->payload = NULL;
    }
    if (data_packet != NULL) {
        free(data_packet);
        data_packet = NULL;
    }
}

void PrintCharPacket(const uint8_t *data, const int data_len){
    // create a string buffer to hold the formatted string
    static const char *TAG = "DATA";
    size_t buf_len = data_len * 3 + 1;
    char *str = (char*)malloc(buf_len);
    if (!str) return;
    char *p = str;
    for (size_t i = 0; i < data_len; i++) {
        int written = snprintf(p, 4, "%02x ", data[i]);
        p += written;
    }
    *p = '\0';
    // print the formatted string
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
    ESP_LOGD(TAG, "%s", str);
    free(str);
}

char *MacToString(const uint8_t *mac_addr)
{
    char *mac_str = (char *)malloc(18); // 17 characters for MAC + null terminator
    if (mac_str == NULL) {
        ESP_LOGE("MAC", "Malloc MAC string fail");
        return NULL;
    }
    snprintf(mac_str, 18, "%02x:%02x:%02x:%02x:%02x:%02x", 
            mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    return mac_str;
}
