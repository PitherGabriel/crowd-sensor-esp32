#ifndef CONFIG_H
#define CONFIG_H

// Sensor identification
#define SENSOR_ID "sensor-001"
#define SENSOR_LOCATION "test_zone_1"
#define SENSOR_LAT 56.1629  // Aarhus coordinates (Smukfest)
#define SENSOR_LON 10.2039

// WiFi sniffing configuration
#define WIFI_CHANNEL_DEFAULT 1
#define WIFI_CHANNEL_HOP_INTERVAL 2000  // ms, cycle through channels
#define WIFI_CHANNELS {1, 6, 11}  // 2.4GHz non-overlapping channels
#define WIFI_CHANNELS_COUNT 3

// Bluetooth scanning
#define BLE_SCAN_INTERVAL 10  // seconds
#define BLE_SCAN_WINDOW 5     // seconds (active scan time)

// Processing parameters
#define MAC_HASH_SALT "CHANGE_ME_RANDOM_STRING"  // Should be unique per deployment
#define MAC_TTL_SECONDS 900  // 15 minutes
#define CORRECTION_FACTOR 1.35  // Adjust based on field testing
#define MIN_RSSI -85  // dBm, filter weak signals

// MQTT configuration
#define MQTT_BROKER "mqtt.yourserver.com"
#define MQTT_PORT 8883  // Use 1883 for non-TLS
#define MQTT_TOPIC "crowd/density"
#define MQTT_QOS 1
#define MQTT_USERNAME "sensor"  // Set if broker requires auth
#define MQTT_PASSWORD "password"
#define PUBLISH_INTERVAL 30  // seconds

// Privacy settings
#define PRIVACY_HASH_MACS true
#define PRIVACY_SALT_ROTATE_INTERVAL 86400  // 24 hours

// NVS storage keys
#define NVS_NAMESPACE "crowd_sensor"
#define NVS_KEY_WIFI_MACS "wifi_macs"
#define NVS_KEY_BT_MACS "bt_macs"

// Power management
#define ENABLE_DEEP_SLEEP false  // Set true for battery operation
#define DEEP_SLEEP_INTERVAL 60  // seconds between wake cycles

#endif  // CONFIG_H
