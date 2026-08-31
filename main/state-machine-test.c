
#include "axolotl_state.h"
#include "button.h"
#include "esp_log.h"
#include "freertos/idf_additions.h"
#include <stdio.h>

static const char *TAG = "AXOLOTL-STATE-TEST";

void main_task(void *pvParameters) {
  while (1) {
    vTaskDelay(pdMS_TO_TICKS(10));
    if (!read_button())
      continue;

    // pressed
    next_state();
    ESP_LOGI(TAG, "%s", get_state_string());

    // wait for release
    while (read_button())
      vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void app_main(void) {
  init_button();
  create_axolotl_task();
  xTaskCreate(main_task, "MainTask", 4096, NULL, 1, NULL);
}
