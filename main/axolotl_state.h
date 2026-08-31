#pragma once

#include <stdint.h>

typedef enum {
  OFF,
  CYCLING,
  STEADY,

  AXOLOTL_STATE_COUNT,
} AXOLOTL_STATE;

// ordered by the order they appear in
// 0 = first
// 1 = second
// 2 = ...
typedef enum {
  WHITE,
  BLUE,
  YELLOW,
  ORANGE,
  PINK,

  AXOLOTL_COLOR_COUNT,
} AXOLOTL_COLOR;

void create_axolotl_task(void);
void next_state(void);
AXOLOTL_STATE get_state(void);
AXOLOTL_COLOR get_color(void);
const char *get_state_string(void);
const char *get_color_string(void);
