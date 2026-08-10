idf_component_register(
SRCS
    "connect.c"

INCLUDE_DIRS 
    "include"

PRIV_REQUIRES
    "main"

REQUIRES
    "asic"
    "nvs_flash"
    "esp_wifi"
    "esp_event"
    "stratum"
    "stratum_v2"
    "espressif__mdns"
)
