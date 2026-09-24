#include "anvl.h"

static const unsigned int gappx = 4; // 0 to disable gap
static const bool show_bar      = true;
static const int barpx          = 16;
static const char *font         = "monospace:size=10";
static const char *kb_layout    = "us";

static const char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };

static Layout layouts[] = {
  { "[]=", tile }, // First entry is default
  { "[M]", monocle },
};

#define CONTROL RIVER_SEAT_V1_MODIFIERS_CTRL
#define SUPER RIVER_SEAT_V1_MODIFIERS_MOD4
#define SHIFT RIVER_SEAT_V1_MODIFIERS_SHIFT
#define ALT RIVER_SEAT_V1_MODIFIERS_MOD1

#define TAGKEY(KEY,TAG) \
  {SUPER,               KEY, view,       { .u = TAG } }, \
  {SUPER|SHIFT,         KEY, tag,        { .u = TAG } },

static const char *termcmd[] = { "foot", NULL };

static Keys keybinds[] = {
  {SUPER|SHIFT,   XKB_KEY_Return, spawn,          {.v = termcmd} },
  {SUPER,         XKB_KEY_t,      set_layout,     {.v = &layouts[0]} },
  {SUPER,         XKB_KEY_m,      set_layout,     {.v = &layouts[1]} },
  {SUPER|CONTROL, XKB_KEY_c,      destroy_window, {0} },
  {SUPER|SHIFT,   XKB_KEY_q,      exit_session,   {0} },
  {SUPER,         XKB_KEY_j,      focus_next,     {0} },
  {SUPER,         XKB_KEY_k,      focus_prev,     {0} },
  {SUPER,         XKB_KEY_period, focus_next_mon, {0} },
  {SUPER,         XKB_KEY_comma,  focus_prev_mon, {0} },
  {SUPER|SHIFT,   XKB_KEY_period, tag_next_mon,   {0} },
  {SUPER|SHIFT,   XKB_KEY_comma,  tag_prev_mon,   {0} },
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

static Buttons buttons[] = {
  {SUPER, BTN_RIGHT, resize, {0} },
};
