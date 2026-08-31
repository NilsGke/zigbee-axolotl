
#include "axolotl_state.h"
#include <stdbool.h>

void set_target_color(AXOLOTL_COLOR color);
void set_target_state(AXOLOTL_STATE state);
AXOLOTL_COLOR get_target_color();
AXOLOTL_STATE get_target_state();
const char *get_target_color_string();
const char *get_target_state_string();

bool should_press_button();
