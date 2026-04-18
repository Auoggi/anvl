#include "anvl.h"

#define CONTROL RIVER_SEAT_V1_MODIFIERS_CTRL
#define SUPER RIVER_SEAT_V1_MODIFIERS_MOD4
#define SHIFT RIVER_SEAT_V1_MODIFIERS_SHIFT

#define TAGKEY(KEY,TAG) \
	{SUPER,               KEY, view,       { .u = 1 << TAG } }, \
	{SUPER|CONTROL,       KEY, toggleview, { .u = 1 << TAG } }, \
	{SUPER|SHIFT,         KEY, tag,        { .u = 1 << TAG } }, \
	{SUPER|SHIFT|CONTROL, KEY, toggletag,  { .u = 1 << TAG } },

const char *footcmd[] = { "foot", NULL };

Keys keybinds[] = {
  {SUPER,         XKB_KEY_period, select_next_mon, {0} },
  {SUPER,         XKB_KEY_comma,  select_prev_mon, {0} },
  {SUPER,         XKB_KEY_n,      focus_next,      {0} },
  {SUPER|SHIFT,   XKB_KEY_n,      focus_prev,      {0} },
  {SUPER|CONTROL, XKB_KEY_c,      destroy_window,  {0} },
  {SUPER|SHIFT,   XKB_KEY_q,      exit_session,    {0} },
  {SUPER,         XKB_KEY_i,      incnmaster,      { .i = +1 } },
  {SUPER,         XKB_KEY_d,      incnmaster,      { .i = -1 } },
  {SUPER,         XKB_KEY_h,      setmfact,        { .f = -0.05 } },
  {SUPER,         XKB_KEY_l,      setmfact,        { .f = +0.05 } },
  {SUPER|SHIFT,   XKB_KEY_Return, spawn,           { .v = footcmd } },
  TAGKEY(XKB_KEY_1, 0)
  TAGKEY(XKB_KEY_2, 1)
  TAGKEY(XKB_KEY_3, 2)
  TAGKEY(XKB_KEY_4, 3)
  TAGKEY(XKB_KEY_5, 4)
  TAGKEY(XKB_KEY_6, 5)
  TAGKEY(XKB_KEY_7, 6)
  TAGKEY(XKB_KEY_8, 7)
  TAGKEY(XKB_KEY_9, 8)
};
