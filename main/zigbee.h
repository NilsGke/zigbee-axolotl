
#pragma once
#include "esp_zigbee.h"

#define AXOLOTL_MANUFACTURER_NAME                                              \
  "\x07"                                                                       \
  "NilsGke"
#define AXOLOTL_MODEL_IDENTIFIER                                               \
  "\x07"                                                                       \
  "Axolotl"
#define AXOLOTL_ENDPOINT 10

#define AXOLOTL_ZB_DEVICE_CONFIG()                                             \
  {                                                                            \
      .device_type = EZB_NWK_DEVICE_TYPE_ROUTER,                               \
      .install_code_policy = false,                                            \
      .zczr_config = {.max_children = 10},                                     \
  }
#define AXOLOTL_ZB_PLATFORM_CONFIG()                                           \
  {                                                                            \
      .storage_partition_name = "nvs",                                         \
      .radio_config = {.radio_mode = ESP_ZIGBEE_RADIO_MODE_NATIVE},            \
  }

#define AXOLOTL_ZB_CONFIG()                                                    \
  {                                                                            \
      .device_config = AXOLOTL_ZB_DEVICE_CONFIG(),                             \
      .platform_config = AXOLOTL_ZB_PLATFORM_CONFIG(),                         \
  }

#define ZIGBEE_CHANNEL_MASK 0x07FFF800

void init_zigbee(void);
