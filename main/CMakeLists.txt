idf_component_register(
    SRCS "main.c" "bt_scanner.c"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash bt
)
