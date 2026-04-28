#ifndef MQTT_CLIENT_CROWD_H
#define MQTT_CLIENT_CROWD_H

#include "density_estimator.h"

// Initialize MQTT client
void mqtt_client_crowd_init(void);

// Publish crowd density data
void mqtt_publish_density(crowd_estimate_t *estimate);

#endif  // MQTT_CLIENT_CROWD_H
