#include "density_estimator.h"
#include "wifi_sniffer.h"
#include "bt_scanner.h"
#include "config.h"
#include "esp_log.h"
#include <time.h>
#include <math.h>

static const char *TAG = "Density_Estimator";

crowd_estimate_t estimate_crowd_density(void) {
    crowd_estimate_t estimate;

    // Get device counts from WiFi and BLE
    estimate.wifi_devices = wifi_sniffer_get_device_count();
    estimate.ble_devices = ble_scanner_get_device_count();

    // Estimate unique devices (conservative: take max to account for overlap)
    // In future: use fingerprinting to deduplicate WiFi+BLE from same device
    estimate.unique_devices = (estimate.wifi_devices > estimate.ble_devices) ?
                               estimate.wifi_devices : estimate.ble_devices;

    // Apply correction factor
    estimate.correction_factor = CORRECTION_FACTOR;
    estimate.estimated_people = (uint16_t)(estimate.unique_devices * estimate.correction_factor);

    // Timestamp
    estimate.timestamp = (uint32_t)time(NULL);

    // Density classification (assume 400 sqm coverage)
    estimate.density_level = get_density_classification(estimate.estimated_people, 400);

    ESP_LOGI(TAG, "Estimate: WiFi=%d, BLE=%d, Unique=%d, People=%d",
             estimate.wifi_devices, estimate.ble_devices,
             estimate.unique_devices, estimate.estimated_people);

    return estimate;
}

density_level_t get_density_classification(uint16_t people, uint16_t area_sqm) {
    float density = (float)people / (float)area_sqm;

    if (density < 2.0) {
        return DENSITY_COMFORTABLE;
    } else if (density < 3.0) {
        return DENSITY_MODERATE;
    } else if (density < 4.0) {
        return DENSITY_CROWDED;
    } else {
        return DENSITY_CRITICAL;
    }
}
