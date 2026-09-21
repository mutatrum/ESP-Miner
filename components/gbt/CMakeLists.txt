idf_component_register(
SRCS
    "coinbase_builder.c"
    "gbt_fsm_parser.c"

INCLUDE_DIRS
    "include"

REQUIRES
    "stratum"
    "esp_psram"
)
