#ifndef WIFI_SNIFFER_H
#define WIFI_SNIFFER_H

#include <stdint.h>
#include "esp_wifi_types.h"

// IEEE 802.11 MAC header (not exposed in newer ESP-IDF public headers)
typedef struct {
    uint8_t frame_ctrl[2];
    uint8_t duration[2];
    uint8_t addr1[6];   // receiver
    uint8_t addr2[6];   // transmitter / source
    uint8_t addr3[6];   // BSSID / filtering address
    uint8_t seq_ctrl[2];
} wifi_ieee80211_mac_hdr_t;

typedef struct {
    wifi_ieee80211_mac_hdr_t hdr;
    uint8_t payload[0];
} wifi_ieee80211_packet_t;

// Device entry in cache
typedef struct {
    uint8_t mac_hash[16];  // Hashed MAC address
    int8_t rssi;           // Signal strength
    uint32_t timestamp;    // Last seen timestamp
    uint8_t is_active;     // 1 if within TTL, 0 if expired
} wifi_device_t;

// Initialize WiFi sniffer
void wifi_sniffer_init(void);

// Start WiFi promiscuous mode
void wifi_sniffer_start(void);

// Stop WiFi sniffer
void wifi_sniffer_stop(void);

// Get unique device count
uint16_t wifi_sniffer_get_device_count(void);

// Clean expired entries
void wifi_sniffer_clean_expired(void);

#endif  // WIFI_SNIFFER_H
