idf_component_register(
    SRCS "main.c" "wifi_sniffer.c" "bt_scanner.c" "density_estimator.c" "mqtt_client_crowd.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_event nvs_flash bt mqtt mbedtls json
)
