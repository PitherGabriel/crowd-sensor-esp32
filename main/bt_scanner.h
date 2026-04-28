#ifndef BT_SCANNER_H
#define BT_SCANNER_H

#include <stdint.h>

// BLE device entry
typedef struct {
    uint8_t mac_hash[16];
    int8_t rssi;
    uint32_t timestamp;
    uint8_t is_active;
} ble_device_t;

// Initialize BLE scanner
void ble_scanner_init(void);

// Start BLE scanning
void ble_scanner_start(void);

// Stop BLE scanning
void ble_scanner_stop(void);

// Get unique BLE device count
uint16_t ble_scanner_get_device_count(void);

// Clean expired entries
void ble_scanner_clean_expired(void);

#endif  // BT_SCANNER_H
