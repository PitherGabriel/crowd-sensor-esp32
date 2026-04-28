#include "mqtt_client_crowd.h"
#include "config.h"
#include "esp_log.h"
#include "mqtt_client.h"  // ESP-IDF MQTT library
#include "cJSON.h"
#include <string.h>

static const char *TAG = "MQTT_Client";
static esp_mqtt_client_handle_t mqtt_client = NULL;

// MQTT event handler
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                                int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Connected to MQTT broker");
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Disconnected from MQTT broker");
            break;

        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "Message published (msg_id=%d)", event->msg_id);
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT error");
            break;

        default:
            break;
    }
}

void mqtt_client_crowd_init(void) {
    // MQTT client configuration
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://" MQTT_BROKER,  // Use mqtts:// for TLS
        .broker.address.port = MQTT_PORT,
        .credentials.username = MQTT_USERNAME,
        .credentials.authentication.password = MQTT_PASSWORD,
        // .broker.verification.certificate = (const char *)mqtt_ca_cert,  // Add for TLS
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);

    ESP_LOGI(TAG, "MQTT client initialized");
}

void mqtt_publish_density(crowd_estimate_t *estimate) {
    if (mqtt_client == NULL) {
        ESP_LOGE(TAG, "MQTT client not initialized");
        return;
    }

    // Create JSON payload
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "sensor_id", SENSOR_ID);
    cJSON_AddStringToObject(root, "location", SENSOR_LOCATION);

    cJSON *coords = cJSON_CreateObject();
    cJSON_AddNumberToObject(coords, "lat", SENSOR_LAT);
    cJSON_AddNumberToObject(coords, "lon", SENSOR_LON);
    cJSON_AddItemToObject(root, "coordinates", coords);

    cJSON_AddNumberToObject(root, "wifi_devices", estimate->wifi_devices);
    cJSON_AddNumberToObject(root, "ble_devices", estimate->ble_devices);
    cJSON_AddNumberToObject(root, "unique_devices", estimate->unique_devices);
    cJSON_AddNumberToObject(root, "estimated_people", estimate->estimated_people);
    cJSON_AddNumberToObject(root, "correction_factor", estimate->correction_factor);
    cJSON_AddNumberToObject(root, "timestamp", estimate->timestamp);

    const char *density_levels[] = {"comfortable", "moderate", "crowded", "critical"};
    cJSON_AddStringToObject(root, "density_level", density_levels[estimate->density_level]);

    // Convert to JSON string
    char *json_string = cJSON_PrintUnformatted(root);

    // Publish
    int msg_id = esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC, json_string,
                                          strlen(json_string), MQTT_QOS, 0);

    ESP_LOGI(TAG, "Published: %d people (msg_id=%d)", estimate->estimated_people, msg_id);

    // Cleanup
    cJSON_Delete(root);
    free(json_string);
}
