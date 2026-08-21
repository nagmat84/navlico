idf_component_register(SRCS "main.c" "navlico_fsm.c" "navlico_pm.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_driver_gpio esp_pm)
