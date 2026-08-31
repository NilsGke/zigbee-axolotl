
#include "axolotl_button.h"

#include "driver/gpio.h"
#include "freertos/idf_additions.h"

#define BUTTON_PIN GPIO_NUM_0

void init_axolotl_button() {
  gpio_reset_pin(BUTTON_PIN);
  ESP_ERROR_CHECK(gpio_set_direction(BUTTON_PIN, GPIO_MODE_OUTPUT));
}

void press_button() {
  gpio_set_level(BUTTON_PIN, 1);
  vTaskDelay(pdMS_TO_TICKS(100));
  gpio_set_level(BUTTON_PIN, 0);
}
