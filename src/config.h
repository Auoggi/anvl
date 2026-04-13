#include "anvl.h"

#define CTRL RIVER_SEAT_V1_MODIFIERS_CTRL
#define SUPER RIVER_SEAT_V1_MODIFIERS_MOD4
#define SHIFT RIVER_SEAT_V1_MODIFIERS_SHIFT

Keys keybinds[] = {
  {SUPER,       XKB_KEY_period, select_next_mon, {0}},
  {SUPER,       XKB_KEY_comma,  select_prev_mon, {0}},
  {SUPER,       XKB_KEY_n,      focus_next,      {0}},
  {SUPER|SHIFT, XKB_KEY_n,      focus_prev,      {0}},
  {SUPER|CTRL,  XKB_KEY_c,      destroy_window,  {0}},
  {SUPER|SHIFT, XKB_KEY_q,      exit_session,    {0}},
  {SUPER,       XKB_KEY_i,      incnmaster,      { .i = +1 }},
  {SUPER,       XKB_KEY_d,      incnmaster,      { .i = -1 }},
  {SUPER,       XKB_KEY_h,      setmfact,        { .f = -0.05 }},
  {SUPER,       XKB_KEY_l,      setmfact,        { .f = +0.05 }},
  {SUPER|SHIFT, XKB_KEY_Return, spawn,           { .v = (const char*[]) { "foot", NULL } }},
};
