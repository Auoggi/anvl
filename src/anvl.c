#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 202405L
#endif

#include <sys/mman.h>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "anvl.h"

#define MIN(A, B) (A < B ? A : B)
#define MAX(A, B) (A > B ? A : B)
#define LENGTH(A) (sizeof A / sizeof A[0])
#define ISVISIBLE(C) (C->tagmask & C->mon->tagmask)
#define CLAMP(VAL, MIN, MAX) VAL = VAL < MIN ? MIN : (VAL > MAX ? MAX : VAL)

WindowManager anvl;
Output *selmon = NULL;

struct xkb_context *xkb_context;
struct river_xkb_config_v1 *xkb_config;
struct river_xkb_keymap_v1 *xkb_keymap;
struct river_xkb_bindings_v1 *xkb_bindings;
struct river_input_manager_v1 *input_manager;
struct river_window_manager_v1 *window_manager;

void destroy_window(Seat *seat, Arg *arg) {
  if(seat->focused != NULL) {
    river_window_v1_close(seat->focused->river_window);
  }
}

void select_next_mon(Seat *seat, Arg *arg) {
  if(selmon != NULL) {
    Output *next = wl_container_of(selmon->link.next, selmon, link);
    if(next != NULL && &next->link != &anvl.outputs) {
      selmon = next;
      river_seat_v1_pointer_warp(seat->river_seat, selmon->x + selmon->width/2, selmon->y + selmon->height/2);
    }
  }
}

void select_prev_mon(Seat *seat, Arg *arg) {
  if(selmon != NULL) {
    Output *prev = wl_container_of(selmon->link.prev, selmon, link);
    if(prev != NULL && &prev->link != &anvl.outputs) {
      selmon = prev;
      river_seat_v1_pointer_warp(seat->river_seat, selmon->x + selmon->width/2, selmon->y + selmon->height/2);
    }
  }
}

void focus_next(Seat *seat, Arg *arg) {
  if(seat->focused != NULL) {
    Window *next = wl_container_of(seat->focused->link.next, seat->focused, link);
    if(next != NULL && &next->link != &anvl.windows) {
      seat->focused = next;
      river_seat_v1_pointer_warp(seat->river_seat, seat->focused->x + seat->focused->width/2, seat->focused->y + seat->focused->height/2);
    }
  }
}

void focus_prev(Seat *seat, Arg *arg) {
  if(seat->focused != NULL) {
    Window *prev = wl_container_of(seat->focused->link.prev, seat->focused, link);
    if(prev != NULL && &prev->link != &anvl.windows) {
      seat->focused = prev;
      river_seat_v1_pointer_warp(seat->river_seat, seat->focused->x + seat->focused->width/2, seat->focused->y + seat->focused->height/2);
    }
  }
}

void incnmaster(Seat *seat, Arg *arg) {
  if(seat->focused != NULL) {
    seat->focused->mon->nmaster += arg->i;
    CLAMP(seat->focused->mon->nmaster, 0, (1 << 16));
  }
}

void setmfact(Seat *seat, Arg *arg) {
  if(seat->focused != NULL) {
    seat->focused->mon->mfact += arg->f;
    CLAMP(seat->focused->mon->mfact, 0, 1);
  }
}

void view(Seat *seat, Arg *arg) {
  if(selmon != NULL) {
    selmon->seltag = arg->u;
    selmon->tagmask = arg->u;
  }
}

void toggleview(Seat *seat, Arg *arg) {
  if(selmon != NULL) {
    selmon->tagmask ^= arg->u;

    if(selmon->tagmask == 0) {
      selmon->tagmask = selmon->seltag;
    }

    // If current selected tag is toggled off, select leftmost viewed tag
    if(arg->u == selmon->seltag) {
      selmon->seltag = selmon->tagmask & -selmon->tagmask;
    }
  }
}

void tag(Seat *seat, Arg *arg) {
  if(seat->focused != NULL) {
    seat->focused->tagmask = arg->u;
  }
}

void toggletag(Seat *seat, Arg *arg) {
  if(seat->focused != NULL) {
    seat->focused->tagmask ^= arg->u;

    if(seat->focused->tagmask == 0) {
      seat->focused->tagmask = selmon->seltag;
    }
  }
}


void exit_session(Seat *seat, Arg *arg) {
  river_window_manager_v1_exit_session(window_manager);
}

void spawn(Seat *seat, Arg *arg) {
  if(fork() == 0) execvp(((char **) arg->v)[0], (char **) arg->v);
}

// include config.h for definition of keybinds
#include "config.h"

void river_output_v1_removed(void *data, struct river_output_v1 *obj) {
  Output *output = data;

  river_output_v1_destroy(output->river_output);
  wl_list_remove(&output->link);
  free(output);
}

void river_output_v1_wl_output(void *data, struct river_output_v1 *obj, uint32_t name) {}

void river_output_v1_position(void *data, struct river_output_v1 *obj, int32_t x, int32_t y) {
  Output *output = data;

  output->x = x;
  output->y = y;
}

void river_output_v1_dimensions(void *data, struct river_output_v1 *obj, int32_t width, int32_t height) {
  Output *output = data;

  output->width = width;
  output->height = height;
}

const struct river_output_v1_listener output_listener = {
  .removed = river_output_v1_removed,
  .wl_output = river_output_v1_wl_output,
  .position = river_output_v1_position,
  .dimensions = river_output_v1_dimensions,
};

void river_window_v1_closed(void *data, struct river_window_v1 *obj) {
  Window *window = data;

  Seat *seat;
  wl_list_for_each(seat, &anvl.seats, link) {
    if(seat->focused == window) {
      seat->focused = NULL;
    }
  }

  river_window_v1_destroy(window->river_window);
  wl_list_remove(&window->link);
  free(window);
}

void river_window_v1_dimensions_hint(void *data, struct river_window_v1 *obj, int32_t min_width, int32_t min_height, int32_t max_width, int32_t max_height) {}

void river_window_v1_dimensions(void *data, struct river_window_v1 *obj, int32_t width, int32_t height) {
  struct Window *window = data;
  window->width = width;
  window->height = height;
}

void river_window_v1_app_id(void *data, struct river_window_v1 *obj, const char *app_id) {}
void river_window_v1_title(void *data, struct river_window_v1 *obj, const char *title) {}
void river_window_v1_parent(void *data, struct river_window_v1 *obj, struct river_window_v1 *parent) {}
void river_window_v1_decoration_hint(void *data, struct river_window_v1 *obj, uint32_t hint) {}
void river_window_v1_pointer_move_requested(void *data, struct river_window_v1 *obj, struct river_seat_v1 *river_seat) {}
void river_window_v1_pointer_resize_requested(void *data, struct river_window_v1 *obj, struct river_seat_v1 *river_seat, uint32_t edges) {}
void river_window_v1_show_window_menu_requested(void *data, struct river_window_v1 *obj, int32_t x, int32_t y) {}
void river_window_v1_maximize_requested(void *data, struct river_window_v1 *obj) {}
void river_window_v1_unmaximize_requested(void *data, struct river_window_v1 *obj) {}
void river_window_v1_fullscreen_requested(void *data, struct river_window_v1 *obj, struct river_output_v1 *river_output) {}
void river_window_v1_exit_fullscreen_requested(void *data, struct river_window_v1 *obj) {}
void river_window_v1_minimize_requested(void *data, struct river_window_v1 *obj) {}
void river_window_v1_unreliable_pid(void *data, struct river_window_v1 *obj, int32_t unreliable_pid) {}
void river_window_v1_presentation_hint(void *data, struct river_window_v1 *obj, uint32_t hint) {}
void river_window_v1_identifier(void *data, struct river_window_v1 *obj, const char *indentifier) {}

const struct river_window_v1_listener window_listener = {
  .closed = river_window_v1_closed,
  .dimensions_hint = river_window_v1_dimensions_hint,
  .dimensions = river_window_v1_dimensions,
  .app_id = river_window_v1_app_id,
  .title = river_window_v1_title,
  .parent = river_window_v1_parent,
  .decoration_hint = river_window_v1_decoration_hint,
  .pointer_move_requested = river_window_v1_pointer_move_requested,
  .pointer_resize_requested = river_window_v1_pointer_resize_requested,
  .show_window_menu_requested = river_window_v1_show_window_menu_requested,
  .maximize_requested = river_window_v1_maximize_requested,
  .unmaximize_requested = river_window_v1_unmaximize_requested,
  .fullscreen_requested = river_window_v1_fullscreen_requested,
  .exit_fullscreen_requested = river_window_v1_exit_fullscreen_requested,
  .minimize_requested = river_window_v1_minimize_requested,
  .unreliable_pid = river_window_v1_unreliable_pid,
  .presentation_hint = river_window_v1_presentation_hint,
  .identifier = river_window_v1_identifier,
};

void window_set_position(Window *window, int x, int y) {
  window->x = x + window->mon->x;
  window->y = y + window->mon->y;
  river_node_v1_set_position(window->river_node, window->x, window->y);
}

void window_set_dimensions(Window *window, int width, int height) {
  window->width = width;
  window->height = height;
  river_window_v1_propose_dimensions(window->river_window, window->width, window->height);
}

void river_xkb_binding_v1_pressed(void *data, struct river_xkb_binding_v1 *obj) {
  Key *key = data;
  key->func(key->seat, key->arg);
}

void river_xkb_binding_v1_released(void *data, struct river_xkb_binding_v1 *obj) {}

const struct river_xkb_binding_v1_listener xkb_binding_listener = {
  .pressed = river_xkb_binding_v1_pressed,
  .released = river_xkb_binding_v1_released,
};

void xkb_binding_create(Seat *seat, uint32_t modifiers, xkb_keysym_t keysym, void (*func)(Seat *seat, Arg *arg), Arg *arg) {
  Key *key = calloc(1, sizeof(Key));
  key->river_xkb_binding = river_xkb_bindings_v1_get_xkb_binding(xkb_bindings, seat->river_seat, keysym, modifiers);
  key->seat = seat;
  key->func = func;
  key->arg = arg;

  river_xkb_binding_v1_add_listener(key->river_xkb_binding, &xkb_binding_listener, key);
  river_xkb_binding_v1_enable(key->river_xkb_binding);

  wl_list_insert(&seat->keys, &key->link);
}

void xkb_binding_destroy(Key *key) {
  river_xkb_binding_v1_destroy(key->river_xkb_binding);
  wl_list_remove(&key->link);
  free(key);
}

void river_pointer_binding_v1_pressed(void *data, struct river_pointer_binding_v1 *obj) {
  ((Button*) data)->pressed = true;
}

void river_pointer_binding_v1_released(void *data, struct river_pointer_binding_v1 *obj) {
  ((Button*) data)->pressed = false;
}

const struct river_pointer_binding_v1_listener pointer_binding_listener = {
  .pressed = river_pointer_binding_v1_pressed,
  .released = river_pointer_binding_v1_released,
};

void pointer_binding_create(Seat *seat, uint32_t modifiers, uint32_t ibutton, void (*func)(Seat *seat, Arg *arg), Arg *arg) {
  Button *button = calloc(1, sizeof(Button));
  button->river_pointer_binding = river_seat_v1_get_pointer_binding(seat->river_seat, ibutton, modifiers);
  button->seat = seat;
  button->func = func;
  button->arg = arg;

  river_pointer_binding_v1_add_listener(button->river_pointer_binding, &pointer_binding_listener, button);
  river_pointer_binding_v1_enable(button->river_pointer_binding);

  wl_list_insert(&seat->buttons, &button->link);
}

void pointer_binding_destroy(Button *button) {
  river_pointer_binding_v1_destroy(button->river_pointer_binding);
  wl_list_remove(&button->link);
  free(button);
}

void river_seat_v1_removed(void *data, struct river_seat_v1 *obj) {
  Seat *seat = data;

  Key *key, *key_tmp;
  wl_list_for_each_safe(key, key_tmp, &seat->keys, link) {
    xkb_binding_destroy(key);
  }

  Button *button, *button_tmp;
  wl_list_for_each_safe(button, button_tmp, &seat->buttons, link) {
    pointer_binding_destroy(button);
  }

  river_seat_v1_destroy(seat->river_seat);
  wl_list_remove(&seat->link);
  free(seat);
}

void river_seat_v1_wl_seat(void *data, struct river_seat_v1 *obj, uint32_t id) {}

void river_seat_v1_pointer_enter(void *data, struct river_seat_v1 *obj, struct river_window_v1 *river_window) {
  Seat *seat = data;
  Window *window = river_window_v1_get_user_data(river_window);

  seat->hovered = window;
  window->hovered = true;
}

void river_seat_v1_pointer_leave(void *data, struct river_seat_v1 *obj) {
  Seat *seat = data;
  Window *window = seat->hovered;

  seat->hovered = NULL;
  window->hovered = false;
}

void river_seat_v1_window_interaction(void *data, struct river_seat_v1 *obj, struct river_window_v1 *river_window) {
  Seat *seat = data;
  Window *window = seat->focused;

  if(window != NULL) window->focused = false;
  window = river_window_v1_get_user_data(river_window);

  seat->focused = window;
  window->focused = true;
}

void river_seat_v1_shell_surface_interaction(void *data, struct river_seat_v1 *obj, struct river_shell_surface_v1 *river_shell_surface) {}
void river_seat_v1_op_delta(void *data, struct river_seat_v1 *obj, int32_t dx, int32_t dy) {} // Used for dragging
void river_seat_v1_op_release(void *data, struct river_seat_v1 *obj) {} // Used for dragging

void river_seat_v1_pointer_position(void *data, struct river_seat_v1 *obj, int32_t x, int32_t y) {
  Output *output;
  wl_list_for_each(output, &anvl.outputs, link) {
    if(x >= output->x && x < output->x + output->width && y >= output->y && y < output->y + output->height) {
      selmon = output;
      return;
    }
  }
  
  selmon = NULL;
}

const struct river_seat_v1_listener seat_listener = {
  .removed = river_seat_v1_removed,
  .wl_seat = river_seat_v1_wl_seat,
  .pointer_enter = river_seat_v1_pointer_enter,
  .pointer_leave = river_seat_v1_pointer_leave,
  .window_interaction = river_seat_v1_window_interaction,
  .shell_surface_interaction = river_seat_v1_shell_surface_interaction,
  .op_delta = river_seat_v1_op_delta,
  .op_release = river_seat_v1_op_release,
  .pointer_position = river_seat_v1_pointer_position,
};

void manage_seat(Seat *seat) {
  if(seat->focused == NULL && !wl_list_empty(&anvl.windows)) {
    seat->focused = wl_container_of(anvl.windows.prev, seat->focused, link);
  }

  if(seat->focused != NULL) {
    river_seat_v1_focus_window(seat->river_seat, seat->focused->river_window);
    river_node_v1_place_top(seat->focused->river_node);
  } else {
    river_seat_v1_clear_focus(seat->river_seat);
  }
}

void render_seat(Seat *seat) {}

void river_window_manager_v1_unavailable(void *data, struct river_window_manager_v1 *obj) {
  fprintf(stderr, "error: Unavailable.\n");
  exit(1);
}

void river_window_manager_v1_finished(void *data, struct river_window_manager_v1 *obj) {
  exit(0);
}

int count_visible_windows(Output *output) {
  int c = 0;
  Window *window;
  wl_list_for_each(window, &anvl.windows, link) {
    if(window->mon == output && ISVISIBLE(window)) c++;
  }

  return c;
}

void river_window_manager_v1_manage_start(void *data, struct river_window_manager_v1 *obj) {
  Window *window;
  wl_list_for_each(window, &anvl.windows, link) {
    river_window_v1_hide(window->river_window);
  }

  Output *output;
  wl_list_for_each(output, &anvl.outputs, link) {
    int i = 0;
    int n = count_visible_windows(output);
    int m = output->nmaster;

    Window *prev;
    wl_list_for_each(window, &anvl.windows, link) {
      if(window->mon == output && ISVISIBLE(window)) {
        river_window_v1_show(window->river_window);
        river_window_v1_use_ssd(window->river_window);
        river_window_v1_set_tiled(window->river_window, 15);

        bool two = m < n && m != 0;

        int si = i < m ? i : i - m;
        int div = two ? (i < m ? m : n - m) : n;
        int even = si % 2 == 0;
        int height = (even ? output->height / div : (output->height + (div - 1)) / div);

        float mfact = two ? output->mfact : 1;

        window_set_position(window, 0, 0);
        window_set_dimensions(window, output->width*mfact, height);

        if(two && i >= m) {
          window_set_position(window, window->width, 0);
          window_set_dimensions(window, output->width - (window->width), window->height);
        }
        
        if(si != 0) {
          window_set_position(window, window->x, prev->y + prev->height);
        }

        prev = window;
        i++;
      }
    }
  }

  Seat *seat;
  wl_list_for_each(seat, &anvl.seats, link) {
    manage_seat(seat);
  }

  river_window_manager_v1_manage_finish(window_manager);
}

void river_window_manager_v1_render_start(void *data, struct river_window_manager_v1 *obj) {
  Seat *seat;
  wl_list_for_each(seat, &anvl.seats, link) {
    render_seat(seat);
  }

  river_window_manager_v1_render_finish(window_manager);
}

void river_window_manager_v1_session_locked(void *data, struct river_window_manager_v1 *obj) {}
void river_window_manager_v1_session_unlocked(void *data, struct river_window_manager_v1 *obj) {}

// TODO: Add window to end of list or reverse render order, and autofocus newly created windows
void river_window_manager_v1_window(void *data, struct river_window_manager_v1 *obj, struct river_window_v1 *river_window) {
  Window *window = calloc(1, sizeof(Window));
  window->river_window = river_window;
  window->river_node = river_window_v1_get_node(window->river_window);
  window->hovered = false;
  window->focused = false;
  window->mon = selmon;
  window->tagmask = selmon->seltag;

  river_window_v1_add_listener(window->river_window, &window_listener, window);
  wl_list_insert(&anvl.windows, &window->link);

  river_window_v1_use_ssd(window->river_window);
  river_window_v1_set_tiled(window->river_window, 15);

  window_set_position(window, 0, 0);
  window_set_dimensions(window, window->mon->width, window->mon->width);
}

void river_window_manager_v1_output(void *data, struct river_window_manager_v1 *obj, struct river_output_v1 *river_output) {
  Output *output = calloc(1, sizeof(Output));
  output->river_output = river_output;
  output->nmaster = 1;
  output->mfact = 0.5f;
  output->seltag = 1;
  output->tagmask = 1;

  river_output_v1_add_listener(output->river_output, &output_listener, output);
  wl_list_insert(&anvl.outputs, &output->link);

  if(selmon == NULL) selmon = output;
}

void river_window_manager_v1_seat(void *data, struct river_window_manager_v1 *obj, struct river_seat_v1 *river_seat) {
  Seat *seat = calloc(1, sizeof(Seat));
  seat->river_seat = river_seat;
  seat->focused = NULL;
  seat->hovered = NULL;

  wl_list_init(&seat->keys);
  wl_list_init(&seat->buttons);

  river_seat_v1_add_listener(seat->river_seat, &seat_listener, seat);
  wl_list_insert(&anvl.seats, &seat->link);

  for(int i = 0; i < LENGTH(keybinds); i++) {
    xkb_binding_create(seat, keybinds[i].mods, keybinds[i].key, keybinds[i].func, &keybinds[i].arg);
  }
}

const struct river_window_manager_v1_listener window_manager_listener = {
  .unavailable = river_window_manager_v1_unavailable,
  .finished = river_window_manager_v1_finished,
  .manage_start = river_window_manager_v1_manage_start,
  .render_start = river_window_manager_v1_render_start,
  .session_locked = river_window_manager_v1_session_locked,
  .session_unlocked = river_window_manager_v1_session_unlocked,
  .window = river_window_manager_v1_window,
  .output = river_window_manager_v1_output,
  .seat = river_window_manager_v1_seat,
};

void river_input_manager_v1_finished(void *data, struct river_input_manager_v1 *river_input_manager_v1) {}
void river_input_manager_v1_input_device(void *data, struct river_input_manager_v1 *river_input_manager_v1, struct river_input_device_v1 *id) {}

const struct river_input_manager_v1_listener input_manager_listener = {
  .finished = river_input_manager_v1_finished,
  .input_device = river_input_manager_v1_input_device,
};

void river_input_device_v1_removed(void *data, struct river_input_device_v1 *river_input_device_v1) {}
void river_input_device_v1_type(void *data, struct river_input_device_v1 *river_input_device_v1, uint32_t type) {}
void river_input_device_v1_name(void *data, struct river_input_device_v1 *river_input_device_v1, const char *name) {}

const struct river_input_device_v1_listener input_device_listener = {
  .removed = river_input_device_v1_removed,
  .type = river_input_device_v1_type,
  .name = river_input_device_v1_name,
};

typedef struct {
  struct river_xkb_keyboard_v1 *river_xkb_keyboard;

  struct wl_list link;
} Keyboard;

void river_xkb_keyboard_v1_removed(void *data, struct river_xkb_keyboard_v1 *river_xkb_keyboard_v1) {
  Keyboard *keyboard = data;

  river_xkb_keyboard_v1_destroy(keyboard->river_xkb_keyboard);
  wl_list_remove(&keyboard->link);
  free(keyboard);
}

void river_xkb_keyboard_v1_input_device(void *data, struct river_xkb_keyboard_v1 *river_xkb_keyboard_v1, struct river_input_device_v1 *device) {}
void river_xkb_keyboard_v1_layout(void *data, struct river_xkb_keyboard_v1 *river_xkb_keyboard_v1, uint32_t index, const char *name) {}
void river_xkb_keyboard_v1_capslock_enabled(void *data, struct river_xkb_keyboard_v1 *river_xkb_keyboard_v1) {}
void river_xkb_keyboard_v1_capslock_disabled(void *data, struct river_xkb_keyboard_v1 *river_xkb_keyboard_v1) {}
void river_xkb_keyboard_v1_numlock_enabled(void *data, struct river_xkb_keyboard_v1 *river_xkb_keyboard_v1) {}
void river_xkb_keyboard_v1_numlock_disabled(void *data, struct river_xkb_keyboard_v1 *river_xkb_keyboard_v1) {}

const struct river_xkb_keyboard_v1_listener xkb_keyboard_listener = {
  .removed = river_xkb_keyboard_v1_removed,
  .input_device = river_xkb_keyboard_v1_input_device,
  .layout = river_xkb_keyboard_v1_layout,
  .capslock_enabled = river_xkb_keyboard_v1_capslock_enabled,
  .capslock_disabled = river_xkb_keyboard_v1_capslock_disabled,
  .numlock_enabled = river_xkb_keyboard_v1_numlock_enabled,
  .numlock_disabled = river_xkb_keyboard_v1_numlock_disabled,
};

void river_xkb_config_v1_finished(void *data, struct river_xkb_config_v1 *river_xkb_config_v1) {
  river_xkb_config_v1_destroy(river_xkb_config_v1);
}

void river_xkb_config_v1_xkb_keyboard(void *data, struct river_xkb_config_v1 *river_xkb_config_v1, struct river_xkb_keyboard_v1 *id) {
  Keyboard *keyboard = calloc(1, sizeof(Keyboard));
  keyboard->river_xkb_keyboard = id;

  wl_list_insert(&anvl.keyboards, &keyboard->link);
  river_xkb_keyboard_v1_add_listener(id, &xkb_keyboard_listener, keyboard);

  if(xkb_keymap) {
    river_xkb_keyboard_v1_set_keymap(keyboard->river_xkb_keyboard, xkb_keymap);
  }
}

const struct river_xkb_config_v1_listener xkb_config_listener = {
  .finished = river_xkb_config_v1_finished,
  .xkb_keyboard = river_xkb_config_v1_xkb_keyboard,
};

// credit to https://git.sr.ht/~zuki/zrwm/tree/afc021dd91bba7a69b1f10fbbf8c5d7bfd66490a/item/zrwm.c#L636
struct river_xkb_keymap_v1* create_keymap() {
  struct xkb_rule_names keymap_rule_names = {0};
  keymap_rule_names.layout = "us";

  struct xkb_keymap *keymap = xkb_keymap_new_from_names2(xkb_context, &keymap_rule_names, XKB_KEYMAP_FORMAT_TEXT_V2, XKB_KEYMAP_COMPILE_NO_FLAGS);
  if(keymap == NULL) {
    fprintf(stderr, "Failed to create xkb keymap\n");
    return NULL;
  }

  char *keymap_str = xkb_keymap_get_as_string2(keymap, XKB_KEYMAP_FORMAT_TEXT_V2, XKB_KEYMAP_SERIALIZE_NO_FLAGS);
  xkb_keymap_unref(keymap);
  int keymap_str_len = strlen(keymap_str) + 1;
  int keymap_fd = memfd_create("anvl-keymap", MFD_CLOEXEC | MFD_ALLOW_SEALING);
  if(keymap_fd == -1 || ftruncate(keymap_fd, keymap_str_len) < 0) {
    fprintf(stderr, "Failed to create or truncate mem fd\n");
    close(keymap_fd);
    free(keymap_str);
    return NULL;
  }

  void *data = mmap(NULL, keymap_str_len, PROT_READ | PROT_WRITE, MAP_SHARED, keymap_fd, 0);
  if(data == MAP_FAILED) {
    fprintf(stderr, "Failed to map data\n");
    close(keymap_fd);
    free(keymap_str);
    return NULL;
  }

  memcpy(data, keymap_str, keymap_str_len);
  free(keymap_str);

  if(munmap(data, keymap_str_len) < 0) {
    fprintf(stderr, "Failed to unmap data\n");
    close(keymap_fd);
    free(keymap_str);
    return NULL;
  }

  if(fcntl(keymap_fd, F_ADD_SEALS, F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_WRITE | F_SEAL_SEAL) < 0) {
    fprintf(stderr, "Failed to seal mem fd\n");
    close(keymap_fd);
    free(keymap_str);
    return NULL;
  }

  return river_xkb_config_v1_create_keymap(xkb_config, keymap_fd, XKB_KEYMAP_FORMAT_TEXT_V2);
}

void river_xkb_keymap_v1_success(void *data, struct river_xkb_keymap_v1 *river_xkb_keymap_v1) {
  Keyboard *keyboard;
  wl_list_for_each(keyboard, &anvl.keyboards, link) {
    river_xkb_keyboard_v1_set_keymap(keyboard->river_xkb_keyboard, xkb_keymap);
  }

  fprintf(stderr, "Successfully created keymap\n");
}

void river_xkb_keymap_v1_failure(void *data, struct river_xkb_keymap_v1 *river_xkb_keymap_v1, const char *error_msg) {
  fprintf(stderr, "Failed to create keymap\n");
}

const struct river_xkb_keymap_v1_listener xkb_keymap_listener = {
  .success = river_xkb_keymap_v1_success,
  .failure = river_xkb_keymap_v1_failure,
};

void wl_registry_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
  if(strcmp(interface, river_window_manager_v1_interface.name) == 0) {
    window_manager = wl_registry_bind(registry, name, &river_window_manager_v1_interface, 4);
    if(window_manager) {
      river_window_manager_v1_add_listener(window_manager, &window_manager_listener, NULL);
    }
  }

  if(strcmp(interface, river_xkb_bindings_v1_interface.name) == 0) {
    xkb_bindings = wl_registry_bind(registry, name, &river_xkb_bindings_v1_interface, 1);
  }

  if(strcmp(interface, river_input_manager_v1_interface.name) == 0) {
    input_manager = wl_registry_bind(registry, name, &river_input_manager_v1_interface, 1);
    if(input_manager != NULL) {
      river_input_manager_v1_add_listener(input_manager, &input_manager_listener, NULL);
    }
  } 

  if(strcmp(interface, river_xkb_config_v1_interface.name) == 0) {
    xkb_config = wl_registry_bind(registry,name,&river_xkb_config_v1_interface, 1);
    if(xkb_config) {
      river_xkb_config_v1_add_listener(xkb_config, &xkb_config_listener, NULL);

      fprintf(stderr, "Trying to create keymap\n");
      xkb_keymap = create_keymap();
      if(xkb_keymap) {
        river_xkb_keymap_v1_add_listener(xkb_keymap, &xkb_keymap_listener, NULL);
      }
    }
  }
}

void wl_registry_global_remove(void *data, struct wl_registry *registry, uint32_t name) {}

const struct wl_registry_listener registry_listener = {
  .global = wl_registry_global,
  .global_remove = wl_registry_global_remove,
};

int main() {
  xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

  struct wl_display *display = wl_display_connect(NULL);
  if(display == NULL) {
    fprintf(stderr, "failed to connect to Wayland server\n");
    return 1;
  }

  signal(SIGCHLD, SIG_IGN);

  struct wl_registry *registry = wl_display_get_registry(display);
  wl_registry_add_listener(registry, &registry_listener, NULL);
  if(wl_display_roundtrip(display) < 0) {
    fprintf(stderr, "roundtrip failed\n");
    return 1;
  }

  if(window_manager == NULL || xkb_bindings == NULL) {
    fprintf(stderr, "river_window_manager_v1 or river_xkb_bindings_v1 not supported by the Wayland server\n");
    return 1;
  }

  wl_list_init(&anvl.keyboards);
  wl_list_init(&anvl.windows);
  wl_list_init(&anvl.outputs);
  wl_list_init(&anvl.seats);

  while(true) {
    if(wl_display_dispatch(display) < 0) {
      fprintf(stderr, "dispatch failed\n");
      return 1;
    }
  }

  return 0;
}
