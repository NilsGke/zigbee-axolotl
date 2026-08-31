
#pragma once

#include "axolotl_state.h"
#include <stdbool.h>

void set_target_color(AXOLOTL_COLOR color);
void set_target_state(AXOLOTL_STATE state);
AXOLOTL_COLOR get_target_color(void);
AXOLOTL_STATE get_target_state(void);
const char *get_target_color_string(void);
const char *get_target_state_string(void);

bool should_press_button(void);
