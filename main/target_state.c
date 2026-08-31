
#include "target_state.h"

#include "axolotl_state.h"
#include <stdbool.h>

static AXOLOTL_COLOR target_color = 0;
static AXOLOTL_STATE target_state = 0;

void set_target_color(AXOLOTL_COLOR color) { target_color = color; }
void set_target_state(AXOLOTL_STATE state) { target_state = state; }
AXOLOTL_COLOR get_target_color() { return target_color; }
AXOLOTL_STATE get_target_state() { return target_state; }
const char *get_target_color_string() { return color_names[target_color]; }
const char *get_target_state_string() { return state_names[target_state]; }

bool should_press_button() {
  AXOLOTL_STATE state = get_state();
  AXOLOTL_COLOR color = get_color();

  switch (target_state) {
  case OFF:
    return state != OFF;
  case CYCLING:
    return state != CYCLING;
  case STATIC:
    if (color == target_color)
      return state != STATIC;
    else
      return state != CYCLING;

  default:
    return false;
  }
}
