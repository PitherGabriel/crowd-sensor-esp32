#include "wifi_sniffer.h"
#include "config.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mbedtls/sha256.h"
#include <string.h>
#include <time.h>

static const char *TAG = "WiFi_Sniffer";

// In-memory cache for detected devices
#define MAX_DEVICES 500
static wifi_device_t device_cache[MAX_DEVICES];
static uint16_t device_count = 0;

// Hash MAC address using SHA256 for privacy
static void hash_mac(const uint8_t *mac, uint8_t *hash_output) {
    mbedtls_sha256_context ctx;
    uint8_t combined[6 + strlen(MAC_HASH_SALT)];

    // Combine MAC + salt
    memcpy(combined, mac, 6);
    memcpy(combined + 6, MAC_HASH_SALT, strlen(MAC_HASH_SALT));

    // SHA256 hash
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);  // 0 = SHA256 (not SHA224)
    mbedtls_sha256_update(&ctx, combined, sizeof(combined));
    uint8_t full_hash[32];
    mbedtls_sha256_finish(&ctx, full_hash);
    mbedtls_sha256_free(&ctx);

    // Take first 16 bytes
    memcpy(hash_output, full_hash, 16);
}

// Find device in cache by hashed MAC
static wifi_device_t* find_device(const uint8_t *mac_hash) {
    for (uint16_t i = 0; i < device_count; i++) {
        if (memcmp(device_cache[i].mac_hash, mac_hash, 16) == 0) {
            return &device_cache[i];
        }
    }
    return NULL;
}

// Add or update device in cache
static void update_device_cache(const uint8_t *mac, int8_t rssi) {
    // Hash the MAC
    uint8_t mac_hash[16];
    hash_mac(mac, mac_hash);

    // Check if already exists
    wifi_device_t *device = find_device(mac_hash);
    uint32_t now = (uint32_t)time(NULL);

    if (device != NULL) {
        // Update existing entry
        device->rssi = rssi;
        device->timestamp = now;
        device->is_active = 1;
    } else {
        // Add new entry (if space available)
        if (device_count < MAX_DEVICES) {
            memcpy(device_cache[device_count].mac_hash, mac_hash, 16);
            device_cache[device_count].rssi = rssi;
            device_cache[device_count].timestamp = now;
            device_cache[device_count].is_active = 1;
            device_count++;

            ESP_LOGI(TAG, "New device detected: %02x%02x... RSSI: %d dBm (total: %d)",
                     mac_hash[0], mac_hash[1], rssi, device_count);
        } else {
            ESP_LOGW(TAG, "Device cache full!");
        }
    }
}

// WiFi packet callback (promiscuous mode)
static void wifi_sniffer_packet_handler(void *buff, wifi_promiscuous_pkt_type_t type) {
    const wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *)buff;
    const wifi_ieee80211_packet_t *ipkt = (wifi_ieee80211_packet_t *)ppkt->payload;
    const wifi_ieee80211_mac_hdr_t *hdr = &ipkt->hdr;

    // Get RSSI
    int8_t rssi = ppkt->rx_ctrl.rssi;

    // Filter weak signals
    if (rssi < MIN_RSSI) {
        return;
    }

    // Extract source MAC address (addr2 = transmitter)
    uint8_t *mac = hdr->addr2;

    // Filter broadcast/multicast
    if (mac[0] & 0x01) {
        return;
    }

    // Update cache
    update_device_cache(mac, rssi);
}

// Channel hopping task
static void channel_hop_task(void *pvParameter) {
    uint8_t channels[] = WIFI_CHANNELS;
    uint8_t channel_idx = 0;

    while (1) {
        esp_wifi_set_channel(channels[channel_idx], WIFI_SECOND_CHAN_NONE);
        ESP_LOGI(TAG, "Switched to channel %d", channels[channel_idx]);

        channel_idx = (channel_idx + 1) % WIFI_CHANNELS_COUNT;
        vTaskDelay(WIFI_CHANNEL_HOP_INTERVAL / portTICK_PERIOD_MS);
    }
}

void wifi_sniffer_init(void) {
    // Initialize WiFi in Station mode
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Set promiscuous mode filter (all packet types)
    wifi_promiscuous_filter_t filter = {
        .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA
    };
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous_filter(&filter));

    // Register packet callback
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous_rx_cb(&wifi_sniffer_packet_handler));

    ESP_LOGI(TAG, "WiFi sniffer initialized");
}

void wifi_sniffer_start(void) {
    // Enable promiscuous mode
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));

    // Start channel hopping task
    xTaskCreate(&channel_hop_task, "channel_hop", 2048, NULL, 5, NULL);

    ESP_LOGI(TAG, "WiFi sniffer started");
}

void wifi_sniffer_stop(void) {
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(false));
    ESP_LOGI(TAG, "WiFi sniffer stopped");
}

uint16_t wifi_sniffer_get_device_count(void) {
    // Clean expired first
    wifi_sniffer_clean_expired();

    // Count active devices
    uint16_t count = 0;
    for (uint16_t i = 0; i < device_count; i++) {
        if (device_cache[i].is_active) {
            count++;
        }
    }
    return count;
}

void wifi_sniffer_clean_expired(void) {
    uint32_t now = (uint32_t)time(NULL);
    uint16_t active_count = 0;

    for (uint16_t i = 0; i < device_count; i++) {
        if ((now - device_cache[i].timestamp) > MAC_TTL_SECONDS) {
            device_cache[i].is_active = 0;  // Mark as expired
        } else {
            active_count++;
        }
    }

    // Optional: Compact the array by removing inactive entries
    // (Skipped for simplicity - devices naturally expire)

    ESP_LOGI(TAG, "Cleaned cache: %d active, %d expired", active_count, device_count - active_count);
}
