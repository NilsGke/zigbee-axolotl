
#include "axolotl_state.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "target_state.h"

static const char *TAG = "AXOLOTL-STATE-TEST";

static AXOLOTL_STATE state = OFF;
static AXOLOTL_COLOR color = 0;
static int64_t cycle_start_time = 0;

static const uint32_t color_duration_ms[AXOLOTL_COLOR_COUNT] = {
    [WHITE] = 4202, [YELLOW] = 4163, [ORANGE] = 2772,
    [PINK] = 3597,  [BLUE] = 4134,
};

void next_state() {

  state = (state + 1) % AXOLOTL_STATE_COUNT;

  if (state == CYCLING) {
    cycle_start_time = esp_timer_get_time() - 1;
    color = WHITE;
  }
}

AXOLOTL_STATE get_state() { return state; }
AXOLOTL_COLOR get_color() { return color; }

const char *get_state_string() { return state_names[state]; }
const char *get_color_string() { return color_names[color]; }

static void tick_task(void *pvParameters) {
  uint16_t total_color_scene_duration = 0;
  for (AXOLOTL_COLOR c = 0; c < AXOLOTL_COLOR_COUNT; c++)
    total_color_scene_duration += color_duration_ms[c];

  // main loop
  while (1) {

    if (state == CYCLING) {
      // update color
      int64_t now = esp_timer_get_time();
      int64_t elapsed_total_ms = (now - cycle_start_time) / 1000;
      int16_t elapsed_curr_cycle_ms =
          elapsed_total_ms % total_color_scene_duration;

      // add color until stack is bigger then elapsed time
      int16_t elapsed_stack_ms = 0;
      AXOLOTL_COLOR curr_color = 0;
      for (; curr_color < AXOLOTL_COLOR_COUNT; curr_color++) {
        elapsed_stack_ms += color_duration_ms[curr_color];
        if (elapsed_curr_cycle_ms < elapsed_stack_ms) {
          // color duration stack surpassed actual elapsed time
          color = curr_color;
          break;
        }
      }
    }

    ESP_LOGI(TAG,
             "\ncurrent color: %s\ncurrent state: %s\ncurrent target color: "
             "%s\ncurrent target state: %s",
             get_color_string(), get_state_string(), get_target_color_string(),
             get_target_state_string());

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void create_axolotl_task() {
  xTaskCreate(tick_task, "AxolotlTickTask", 4096, NULL, 1, NULL);
}
