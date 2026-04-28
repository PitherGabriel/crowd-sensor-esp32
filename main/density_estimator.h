#ifndef DENSITY_ESTIMATOR_H
#define DENSITY_ESTIMATOR_H

#include <stdint.h>

typedef enum {
    DENSITY_COMFORTABLE = 0,  // < 2 people/sqm
    DENSITY_MODERATE = 1,     // 2-3 people/sqm
    DENSITY_CROWDED = 2,      // 3-4 people/sqm
    DENSITY_CRITICAL = 3      // > 4 people/sqm
} density_level_t;

typedef struct {
    uint16_t wifi_devices;
    uint16_t ble_devices;
    uint16_t unique_devices;
    uint16_t estimated_people;
    float correction_factor;
    density_level_t density_level;
    uint32_t timestamp;
} crowd_estimate_t;

// Estimate crowd density
crowd_estimate_t estimate_crowd_density(void);

// Get density classification
density_level_t get_density_classification(uint16_t people, uint16_t area_sqm);

#endif  // DENSITY_ESTIMATOR_H
