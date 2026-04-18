#ifndef ANVLH
#define ANVLH 

#include <wayland-client-core.h>
#include <wayland-client-protocol.h>

#include <linux/input-event-codes.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include <river-xkb-config-v1-client-protocol.h>
#include <river-xkb-bindings-v1-client-protocol.h>
#include <river-input-management-v1-client-protocol.h>
#include <river-window-management-v1-client-protocol.h>

typedef struct Window Window;
typedef struct Output Output;
typedef struct Seat Seat;

struct Window {
  struct river_window_v1 *river_window;
  struct river_node_v1 *river_node;
  struct wl_list link;

  int x;
  int y;

  int width;
  int height;

  bool focused;
  bool hovered;

  Output *mon;
};

struct Output {
  struct river_output_v1 *river_output;
  struct wl_list link;

  int x;
  int y;

  int width;
  int height;

  int nmaster;
  float mfact;
};

struct Seat {
  struct river_seat_v1 *river_seat;
  struct wl_list link;

  Window *focused;
  Window *hovered;

  struct wl_list keys;
  struct wl_list buttons;
};

typedef struct {
  struct wl_list windows;
  struct wl_list outputs;
  struct wl_list seats;
  struct wl_list keyboards;
} WindowManager;

typedef union {
  int i;
  float f;
  void *v;
} Arg;

typedef struct {
  struct river_xkb_binding_v1 *river_xkb_binding;
  struct wl_list link;

  Seat *seat;

  void (*func)(Seat *seat, Arg *arg);
  Arg *arg;
} Key;

typedef struct {
  struct river_pointer_binding_v1 *river_pointer_binding;
  struct wl_list link;

  Seat *seat;

  bool pressed;

  void (*func)(Seat *seat, Arg *arg);
  Arg *arg;
} Button;

typedef struct {
  uint32_t mods;
  xkb_keysym_t key;
  void (*func)(Seat *seat, Arg *arg);
  Arg arg;
} Keys;


void destroy_window(Seat *seat, Arg *arg);
void select_next_mon(Seat *seat, Arg *arg);
void select_prev_mon(Seat *seat, Arg *arg);
void focus_next(Seat *seat, Arg *arg);
void focus_prev(Seat *esat, Arg *arg);
void incnmaster(Seat *seat, Arg *arg);
void setmfact(Seat *seat, Arg *arg);
void exit_session(Seat *seat, Arg *arg);
void spawn(Seat *seat, Arg *arg);

#endif /* ANVLH */
