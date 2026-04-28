#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_sntp.h"

#include "config.h"
#include "wifi_sniffer.h"
#include "bt_scanner.h"
#include "density_estimator.h"
#include "mqtt_client_crowd.h"

static const char *TAG = "Main";

// WiFi credentials for internet connection (for MQTT)
// NOTE: WiFi sniffer uses promiscuous mode separately
#define WIFI_SSID "your_wifi_ssid"
#define WIFI_PASSWORD "your_wifi_password"

// WiFi event handler
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "WiFi disconnected, reconnecting...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

// Initialize WiFi connection for internet (separate from sniffing)
static void wifi_init_sta(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi station initialized");
}

// Initialize SNTP for time synchronization
static void sntp_init(void) {
    ESP_LOGI(TAG, "Initializing SNTP");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();

    // Wait for time to be set
    time_t now = 0;
    struct tm timeinfo = {0};
    int retry = 0;
    const int retry_count = 10;

    while (esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
    time(&now);
    localtime_r(&now, &timeinfo);
}

// Main publishing task
static void mqtt_publish_task(void *pvParameters) {
    while (1) {
        // Estimate crowd density
        crowd_estimate_t estimate = estimate_crowd_density();

        // Publish via MQTT
        mqtt_publish_density(&estimate);

        // Wait for next publish interval
        vTaskDelay((PUBLISH_INTERVAL * 1000) / portTICK_PERIOD_MS);
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "=== Crowd Density Sensor Starting ===");
    ESP_LOGI(TAG, "Sensor ID: %s", SENSOR_ID);
    ESP_LOGI(TAG, "Location: %s", SENSOR_LOCATION);

    // Initialize NVS (for WiFi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize WiFi for internet connection
    wifi_init_sta();

    // Wait for WiFi connection
    vTaskDelay(5000 / portTICK_PERIOD_MS);

    // Initialize SNTP for time sync
    sntp_init();

    // Initialize WiFi sniffer (promiscuous mode)
    wifi_sniffer_init();
    wifi_sniffer_start();

    // Initialize BLE scanner
    ble_scanner_init();
    ble_scanner_start();

    // Initialize MQTT client
    mqtt_client_crowd_init();

    // Wait for MQTT connection
    vTaskDelay(3000 / portTICK_PERIOD_MS);

    // Start MQTT publishing task
    xTaskCreate(&mqtt_publish_task, "mqtt_publish", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "=== Sensor Running ===");
}
