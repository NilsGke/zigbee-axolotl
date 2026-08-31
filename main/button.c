
#include "button.h"

#include "driver/gpio.h"
#include "soc/gpio_num.h"

#define BUTTON_PIN GPIO_NUM_10

void init_button(void) {
  gpio_reset_pin(BUTTON_PIN);
  gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);
  gpio_pullup_en(BUTTON_PIN);
  gpio_pulldown_dis(BUTTON_PIN);
}

bool read_button() { return gpio_get_level(BUTTON_PIN) == 0; }
