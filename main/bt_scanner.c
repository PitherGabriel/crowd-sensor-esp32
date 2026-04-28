#include "bt_scanner.h"
#include "config.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_bt_main.h"
#include "esp_log.h"
#include "mbedtls/sha256.h"
#include <string.h>
#include <time.h>

static const char *TAG = "BLE_Scanner";

#define MAX_BLE_DEVICES 500
static ble_device_t ble_device_cache[MAX_BLE_DEVICES];
static uint16_t ble_device_count = 0;

// Hash MAC address
static void hash_mac(const uint8_t *mac, uint8_t *hash_output) {
    mbedtls_sha256_context ctx;
    uint8_t combined[6 + strlen(MAC_HASH_SALT)];

    memcpy(combined, mac, 6);
    memcpy(combined + 6, MAC_HASH_SALT, strlen(MAC_HASH_SALT));

    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, combined, sizeof(combined));
    uint8_t full_hash[32];
    mbedtls_sha256_finish(&ctx, full_hash);
    mbedtls_sha256_free(&ctx);

    memcpy(hash_output, full_hash, 16);
}

// Find BLE device in cache
static ble_device_t* find_ble_device(const uint8_t *mac_hash) {
    for (uint16_t i = 0; i < ble_device_count; i++) {
        if (memcmp(ble_device_cache[i].mac_hash, mac_hash, 16) == 0) {
            return &ble_device_cache[i];
        }
    }
    return NULL;
}

// Update BLE device cache
static void update_ble_cache(const uint8_t *mac, int8_t rssi) {
    uint8_t mac_hash[16];
    hash_mac(mac, mac_hash);

    ble_device_t *device = find_ble_device(mac_hash);
    uint32_t now = (uint32_t)time(NULL);

    if (device != NULL) {
        device->rssi = rssi;
        device->timestamp = now;
        device->is_active = 1;
    } else {
        if (ble_device_count < MAX_BLE_DEVICES) {
            memcpy(ble_device_cache[ble_device_count].mac_hash, mac_hash, 16);
            ble_device_cache[ble_device_count].rssi = rssi;
            ble_device_cache[ble_device_count].timestamp = now;
            ble_device_cache[ble_device_count].is_active = 1;
            ble_device_count++;

            ESP_LOGI(TAG, "New BLE device: %02x%02x... RSSI: %d dBm (total: %d)",
                     mac_hash[0], mac_hash[1], rssi, ble_device_count);
        }
    }
}

// GAP callback for scan results
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
        case ESP_GAP_BLE_SCAN_RESULT_EVT: {
            esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;

            if (scan_result->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {
                int8_t rssi = scan_result->scan_rst.rssi;

                // Filter weak signals
                if (rssi < MIN_RSSI) {
                    break;
                }

                uint8_t *bda = scan_result->scan_rst.bda;
                update_ble_cache(bda, rssi);
            }
            break;
        }

        case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
            ESP_LOGI(TAG, "BLE scan stopped");
            break;

        default:
            break;
    }
}

void ble_scanner_init(void) {
    // Initialize Bluetooth controller
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));

    // Initialize Bluedroid stack
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    // Register GAP callback
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_event_handler));

    ESP_LOGI(TAG, "BLE scanner initialized");
}

void ble_scanner_start(void) {
    // Configure scan parameters
    esp_ble_scan_params_t scan_params = {
        .scan_type = BLE_SCAN_TYPE_ACTIVE,
        .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
        .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
        .scan_interval = 0x50,  // 50ms
        .scan_window = 0x30,    // 30ms
        .scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE
    };

    ESP_ERROR_CHECK(esp_ble_gap_set_scan_params(&scan_params));
    ESP_ERROR_CHECK(esp_ble_gap_start_scanning(BLE_SCAN_INTERVAL));

    ESP_LOGI(TAG, "BLE scanner started");
}

void ble_scanner_stop(void) {
    ESP_ERROR_CHECK(esp_ble_gap_stop_scanning());
}

uint16_t ble_scanner_get_device_count(void) {
    ble_scanner_clean_expired();

    uint16_t count = 0;
    for (uint16_t i = 0; i < ble_device_count; i++) {
        if (ble_device_cache[i].is_active) {
            count++;
        }
    }
    return count;
}

void ble_scanner_clean_expired(void) {
    uint32_t now = (uint32_t)time(NULL);
    uint16_t active_count = 0;

    for (uint16_t i = 0; i < ble_device_count; i++) {
        if ((now - ble_device_cache[i].timestamp) > MAC_TTL_SECONDS) {
            ble_device_cache[i].is_active = 0;
        } else {
            active_count++;
        }
    }

    ESP_LOGI(TAG, "BLE cache: %d active, %d expired", active_count, ble_device_count - active_count);
}
