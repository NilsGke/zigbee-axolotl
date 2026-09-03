idf_component_register(SRCS "main.c" "button.c" "axolotl_state.c" "axolotl_button.c" "target_state.c" "zigbee.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES espressif__esp-zigbee-lib nvs_flash vfs esp_timer esp_driver_gpio)
