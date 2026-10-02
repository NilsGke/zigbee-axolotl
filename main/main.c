
#include "axolotl_state.h"

#include "axolotl_button.h"
#include "zigbee.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "target_state.h"

static const char *TAG = "AXOLOTL-STATE-TEST";

// XIAO ESP32C6 RF switch: GPIO3 powers the switch (active low),
// GPIO14 selects the antenna (0 = onboard ceramic, 1 = external U.FL)
#define RF_SWITCH_ENABLE_PIN GPIO_NUM_3
#define RF_ANTENNA_SELECT_PIN GPIO_NUM_14

static void init_antenna(void) {
  ESP_ERROR_CHECK(gpio_reset_pin(RF_SWITCH_ENABLE_PIN));
  ESP_ERROR_CHECK(gpio_set_direction(RF_SWITCH_ENABLE_PIN, GPIO_MODE_OUTPUT));
  ESP_ERROR_CHECK(gpio_set_level(RF_SWITCH_ENABLE_PIN, 0));

  ESP_ERROR_CHECK(gpio_reset_pin(RF_ANTENNA_SELECT_PIN));
  ESP_ERROR_CHECK(gpio_set_direction(RF_ANTENNA_SELECT_PIN, GPIO_MODE_OUTPUT));
  ESP_ERROR_CHECK(gpio_set_level(RF_ANTENNA_SELECT_PIN, 0));
}

static void axolotl_control_task(void *pvParameters) {
  while (1) {
    vTaskDelay(pdMS_TO_TICKS(200));
    if (should_press_button()) {
      ESP_LOGI(TAG, "pressed button");
      next_state();   // update virtual model
      press_button(); // update physical axolotl
    }
  }
}

void app_main(void) {
  init_antenna();
  init_axolotl_button();

  init_zigbee();
  create_axolotl_task();

  xTaskCreate(axolotl_control_task, "AxolotlControlTask", 4096, NULL, 1, NULL);
}
