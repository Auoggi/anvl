#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "anvl.h"

#define MIN(A, B) (A < B ? A : B)
#define MAX(A, B) (A > B ? A : B)
#define LENGTH(A) (sizeof A / sizeof A[0])

WindowManager anvl;
Output *selmon = NULL;

struct river_window_manager_v1 *window_manager;
struct river_xkb_bindings_v1 *xkb_bindings;

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
  }
}

void setmfact(Seat *seat, Arg *arg) {
  if(seat->focused != NULL) {
    seat->focused->mon->mfact += arg->f;
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

// TODO: clean up calculation here
void river_window_manager_v1_manage_start(void *data, struct river_window_manager_v1 *obj) {
  Output *output;
  wl_list_for_each(output, &anvl.outputs, link) {
    int i = 0;
    int n = 0;
    int m = output->nmaster;
    Window *window;
    wl_list_for_each(window, &anvl.windows, link) {
      if(window->mon == output) n++;
    }
    m = MAX(0, MIN(n, m));
    wl_list_for_each(window, &anvl.windows, link) {
      if(window->mon == output) {
        river_window_v1_use_ssd(window->river_window);
        river_window_v1_set_tiled(window->river_window, 15);
        if(n == m || m == 0) {
          window_set_position(window, 0, i*(output->height/n));
          window_set_dimensions(window, output->width, output->height/n);
        } else if(i < m) {
          window_set_position(window, 0, i*(output->height/m));
          window_set_dimensions(window, output->width*output->mfact, output->height/m);
        } else {
          window_set_position(window, output->width*output->mfact, (i-m)*(output->height/(n-m)));
          window_set_dimensions(window, output->width - (output->width*output->mfact), output->height/(n-m));
        }

        if(i == m - 1) {
          window_set_dimensions(window, window->width, output->height - ((m-1) * (output->height/m)));
        } else if(i == n - 1) {
          window_set_dimensions(window, window->width, output->height - (((n-m) - 1) * (output->height/(n-m))));
        }

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

void wl_registry_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
  if(strcmp(interface, river_window_manager_v1_interface.name) == 0 && version >= 4) {
    window_manager = wl_registry_bind(registry, name, &river_window_manager_v1_interface, 4);
  }

  if(strcmp(interface, river_xkb_bindings_v1_interface.name) == 0) {
    xkb_bindings = wl_registry_bind(registry, name, &river_xkb_bindings_v1_interface, 1);
  }
}

void wl_registry_global_remove(void *data, struct wl_registry *registry, uint32_t name) {}

const struct wl_registry_listener registry_listener = {
  .global = wl_registry_global,
  .global_remove = wl_registry_global_remove,
};

int main() {
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

  wl_list_init(&anvl.windows);
  wl_list_init(&anvl.outputs);
  wl_list_init(&anvl.seats);

  river_window_manager_v1_add_listener(window_manager, &window_manager_listener, NULL);

  while(true) {
    if(wl_display_dispatch(display) < 0) {
      fprintf(stderr, "dispatch failed\n");
      return 1;
    }
  }

  return 0;
}
