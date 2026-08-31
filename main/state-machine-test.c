
#include "axolotl_state.h"

#include "axolotl_button.h"
#include "button.h"
#include "esp_log.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "target_state.h"
#include <time.h>

static const char *TAG = "AXOLOTL-STATE-TEST";

void physical_button_task(void *pvParameters) {
  while (1) {
    vTaskDelay(pdMS_TO_TICKS(10));
    if (!read_button())
      continue;

    // pressed

    // randomize target color and target state
    set_target_color(rand() % AXOLOTL_COLOR_COUNT);
    set_target_state(rand() % AXOLOTL_STATE_COUNT);

    // wait for release
    while (read_button())
      vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void axolotl_control_task(void *pvParameters) {
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
  srand(time(NULL)); // random initialization
  init_button();
  init_axolotl_button();
  create_axolotl_task();
  xTaskCreate(physical_button_task, "PhysicalButtonTask", 4096, NULL, 1, NULL);
  xTaskCreate(axolotl_control_task, "AxolotlControlTask", 4096, NULL, 1, NULL);
}
