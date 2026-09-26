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

#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include <fcft/fcft.h>

#include "anvl.h"
#include "config.h"

#define MIN(A, B) (A < B ? A : B)
#define MAX(A, B) (A > B ? A : B)
#define LENGTH(A) (sizeof A / sizeof A[0])
#define CLAMP(VAL, MIN, MAX) VAL = VAL < MIN ? MIN : (VAL > MAX ? MAX : VAL)

WindowManager anvl;
Output *selmon = NULL;

struct wl_shm *shm;
struct wl_compositor *compositor;
struct zwlr_layer_shell_v1 *zwlr_layer_shell;

struct fcft_font *fcft_font = NULL;

struct xkb_context *xkb_context;
struct river_xkb_config_v1 *xkb_config;
struct river_xkb_keymap_v1 *xkb_keymap;
struct river_layer_shell_v1 *layer_shell;
struct river_xkb_bindings_v1 *xkb_bindings;
struct river_input_manager_v1 *input_manager;
struct river_window_manager_v1 *window_manager;

void destroy_window(Seat *seat, Arg *arg) {
  if(seat->focused != NULL) {
    river_window_v1_close(seat->focused->river_window);
  }
}

void focus_next_mon(Seat *seat, Arg *arg) {
  if(selmon != NULL) {
    Output *next = wl_container_of(selmon->link.next, selmon, link);
    if(next != NULL && &next->link != &anvl.outputs) {
      selmon = next;
      river_layer_shell_output_v1_set_default(selmon->river_layer_shell);
      river_seat_v1_pointer_warp(seat->river_seat, selmon->x + selmon->width/2, selmon->y + selmon->height/2);
    }
  }
}

void focus_prev_mon(Seat *seat, Arg *arg) {
  if(selmon != NULL) {
    Output *prev = wl_container_of(selmon->link.prev, selmon, link);
    if(prev != NULL && &prev->link != &anvl.outputs) {
      selmon = prev;
      river_layer_shell_output_v1_set_default(selmon->river_layer_shell);
      river_seat_v1_pointer_warp(seat->river_seat, selmon->x + selmon->width/2, selmon->y + selmon->height/2);
    }
  }
}

void tag_next_mon(Seat *seat, Arg *arg) {
  if(selmon != NULL && seat->focused != NULL) {
    Output *next = wl_container_of(selmon->link.next, selmon, link);
    if(next != NULL && &next->link != &anvl.outputs) {
      remove_node(seat->focused->node);
      insert_node(seat->focused, next->tags[next->seltag]->root, next->tags[next->seltag]->focused);
    }
  }
}

void tag_prev_mon(Seat *seat, Arg *arg) {
  if(selmon != NULL && seat->focused != NULL) {
    Output *prev = wl_container_of(selmon->link.prev, selmon, link);
    if(prev != NULL && &prev->link != &anvl.outputs) {
      remove_node(seat->focused->node);
      insert_node(seat->focused, prev->tags[prev->seltag]->root, prev->tags[prev->seltag]->focused);
    }
  }
}

void exit_session(Seat *seat, Arg *arg) {
  river_window_manager_v1_exit_session(window_manager);
}

void set_layout(Seat *seat, Arg *arg) {
  if(selmon != NULL) {
    selmon->tags[selmon->seltag]->lt = arg->v;
  }
}

// TODO: loop and only through windows on selmon
void focus_next(Seat *seat, Arg *arg) {
  if(seat->focused != NULL) {
    Window *next = wl_container_of(seat->focused->link.next, seat->focused, link);
    if(next != NULL && &next->link != &anvl.windows) {
      seat->focused = next;
      river_seat_v1_pointer_warp(seat->river_seat, seat->focused->x + seat->focused->width/2, seat->focused->y + seat->focused->height/2);
    }
  }
}

// TODO: loop and only through windows on selmon
void focus_prev(Seat *seat, Arg *arg) {
  if(seat->focused != NULL) {
    Window *prev = wl_container_of(seat->focused->link.prev, seat->focused, link);
    if(prev != NULL && &prev->link != &anvl.windows) {
      seat->focused = prev;
      river_seat_v1_pointer_warp(seat->river_seat, seat->focused->x + seat->focused->width/2, seat->focused->y + seat->focused->height/2);
    }
  }
}

void view(Seat *seat, Arg *arg) {
  if(selmon != NULL) {
    selmon->seltag = arg->u;
  }
}

void tag(Seat *seat, Arg *arg) {
  if(seat->focused != NULL) {
    remove_node(seat->focused->node);
    insert_node(seat->focused, selmon->tags[arg->u]->root, selmon->tags[arg->u]->focused);
  }
}

void spawn(Seat *seat, Arg *arg) {
  if(fork() == 0) execvp(((char **) arg->v)[0], (char **) arg->v);
}

void resize(Seat *seat, Arg *arg) {
  if(seat->focused == NULL || seat->focused->node == NULL) return;
  if(seat->focused->node->parent == NULL) return;
  river_seat_v1_op_start_pointer(seat->river_seat);
  seat->op = TILE_RESIZE;

  bool right = seat->px > seat->focused->node->x + (seat->focused->node->width >> 1);
  bool bottom = seat->py > seat->focused->node->y + (seat->focused->node->height >> 1);

  seat->op_vnode = NULL;
  seat->op_hnode = NULL;

  Node *last = seat->focused->node;
  Node *next = last->parent;
  while(next != NULL) {
    if(next->split_type == VERTICAL && seat->op_vnode == NULL) {
      if(right && next->first == last) seat->op_vnode = next;
      if(!right && next->second == last) seat->op_vnode = next;
    }

    if(next->split_type == HORIZONTAL && seat->op_hnode == NULL) {
      if(bottom && next->first == last) seat->op_hnode = next;
      if(!bottom && next->second == last) seat->op_hnode = next;
    }

    last = next;
    next = next->parent;
  }

  if(seat->op_vnode != NULL) seat->op_vratio = seat->op_vnode->split_ratio;
  if(seat->op_hnode != NULL) seat->op_hratio = seat->op_hnode->split_ratio;
}

Node *create_node(Tag *tag, Window *window, Node *parent) {
  Node *node = calloc(1, sizeof(Node));
  node->tag = tag;
  node->split_type = UNSET;
  node->split_ratio = 0.5;
  node->window = window;
  node->first = node->second = NULL;
  node->parent = parent;

  return node;
}

// Insert window after ref, which has to be a leaf node with a window attached, by splitting it
// If ref is NULL, the tree is assumed to be empty and window is inserted on root
// TODO: don't allow windows to be smaller than 16x16 pixels
void insert_node(Window *window, Node *root, Node *ref) {
  if(ref == NULL) {
    root->window = window;
    window->node = root;
    root->tag->focused = root;
  } else {
    Window *ref_window = ref->window;
    ref->window = NULL;

    if(ref->split_type == UNSET) {
      if(ref->width >= ref->height) ref->split_type = VERTICAL;
      else ref->split_type = HORIZONTAL;
    }

    Node *first = create_node(root->tag, ref_window, ref);
    ref_window->node = first;

    Node *second = create_node(root->tag, window, ref);
    window->node = second;

    ref->first = first;
    ref->second = second;
    root->tag->focused = second;
  }
}

void remove_node(Node *node) {
  Node *parent = node->parent;
  if(parent == NULL) {
    node->tag->focused = NULL;
    node->split_type = UNSET;
    node->split_ratio = 0.5;
    node->window = NULL;
  } else {
    Node *sibling = parent->first != node ? parent->first : parent->second;

    parent->split_type = sibling->split_type;
    parent->split_ratio = sibling->split_ratio;
    parent->first = sibling->first;
    if(parent->first != NULL) parent->first->parent = parent;
    parent->second = sibling->second;
    if(parent->second != NULL) parent->second->parent = parent;
    parent->window = sibling->window;
    if(parent->window != NULL) parent->window->node = parent;

    free(node);
    free(sibling);
    // TODO: at this point focused is now invalid, it does not seem as if this is a problem
    //  as it is reassigned on the following manage sequence, however this should still be fixed.
  }
}

void propogate_layout(Node *root) {
  Node *queue[1 << 16];
  uint32_t front = 0;
  uint32_t back = 0;

  queue[back++] = root;

  Node *n;
  while(front != back) {
    n = queue[front++];
    if(n->first != NULL && n->second != NULL) {
      queue[back++] = n->first;
      queue[back++] = n->second;

      n->first->x = n->x;
      n->first->y = n->y;
      n->first->width = n->split_type == VERTICAL ? n->width * n->split_ratio - (gappx >> 1) : n->width;
      n->first->height = n->split_type == HORIZONTAL ? n->height * n->split_ratio - (gappx >> 1) : n->height;

      n->second->x = n->split_type == VERTICAL ? n->x + n->first->width + gappx : n->x;
      n->second->y = n->split_type == HORIZONTAL ? n->y + n->first->height + gappx : n->y;
      n->second->width = n->split_type == VERTICAL ? n->width - n->first->width - gappx : n->width;
      n->second->height = n->split_type == HORIZONTAL ? n->height - n->first->height - gappx : n->height;
    }
  }
}

// TODO: reconsider how windows are treated here
void river_output_v1_removed(void *data, struct river_output_v1 *obj) {
  Output *output = data;

  river_layer_shell_output_v1_destroy(output->river_layer_shell);
  river_output_v1_destroy(output->river_output);
  wl_list_remove(&output->link);

  for(int i = 0; i < LENGTH(output->tags); i++) {
    Tag *tag = output->tags[i];

    Node *queue[1 << 16];
    uint32_t front = 0;
    uint32_t back = 0;

    queue[back++] = tag->root;

    Node *n;
    while(front != back) {
      n = queue[front++];
      if(n->first != NULL && n->second != NULL) {
        queue[back++] = n->first;
        queue[back++] = n->second;
      } else if(n->window != NULL) {
        n->window->node = NULL;
      }

      free(n);
    }

    free(tag);
  }

  free(output);
}

void river_output_v1_wl_output(void *data, struct river_output_v1 *obj, uint32_t name) {
  Output *output = data;

  WlOutput *wl_output;
  wl_list_for_each(wl_output, &anvl.wl_outputs, link) {
    if(wl_output->name == name) {
      wl_output->output = output;
      wl_output->done = true;
    }
  }
}

void river_output_v1_position(void *data, struct river_output_v1 *obj, int32_t x, int32_t y) {
  Output *output = data;

  output->x = x;
  output->y = y;

  for(int i = 0; i < LENGTH(output->tags); i++) {
    Tag *tag = output->tags[i];

    tag->root->x = x + gappx;
    tag->root->y = y + (show_bar ? barpx : 0) + gappx;
  }
}

void river_output_v1_dimensions(void *data, struct river_output_v1 *obj, int32_t width, int32_t height) {
  Output *output = data;

  output->width = width;
  output->height = height;

  for(int i = 0; i < LENGTH(output->tags); i++) {
    Tag *tag = output->tags[i];

    tag->root->width = width - 2*gappx;
    tag->root->height = height - (show_bar ? barpx : 0) - 2*gappx;
  }
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

  remove_node(window->node);

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
  window->x = x;
  window->y = y;
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
  Button *button = data;
  button->func(button->seat, button->arg);
}

void river_pointer_binding_v1_released(void *data, struct river_pointer_binding_v1 *obj) {}

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

  seat->focused = window;
}

void river_seat_v1_pointer_leave(void *data, struct river_seat_v1 *obj) {}

void river_seat_v1_window_interaction(void *data, struct river_seat_v1 *obj, struct river_window_v1 *river_window) {
  Seat *seat = data;
  Window *window = seat->focused;

  window = river_window_v1_get_user_data(river_window);

  seat->focused = window;
}

void river_seat_v1_shell_surface_interaction(void *data, struct river_seat_v1 *obj, struct river_shell_surface_v1 *river_shell_surface) {}

// TODO: only resize relevant as to not change position of unrelated corners of window being resized
void river_seat_v1_op_delta(void *data, struct river_seat_v1 *obj, int32_t dx, int32_t dy) {
  Seat *seat = data;

  if(seat->op == TILE_RESIZE) {
    if(seat->op_vnode != NULL) {
      int w = seat->op_vnode->width * seat->op_vratio + dx;
      CLAMP(w, 16, seat->op_vnode->width - 16);
      seat->op_vnode->split_ratio = (double) w / (double) seat->op_vnode->width;
    }

    if(seat->op_hnode != NULL) {
      int h = seat->op_hnode->height * seat->op_hratio + dy;
      CLAMP(h, 16, seat->op_hnode->height - 16);
      seat->op_hnode->split_ratio = (double) h / (double) seat->op_hnode->height;
    }
  }
}

void river_seat_v1_op_release(void *data, struct river_seat_v1 *obj) {
  Seat *seat = data;

  river_seat_v1_op_end(seat->river_seat);
  seat->op = NONE;
  seat->op_vnode = NULL;
  seat->op_hnode = NULL;
}

void river_seat_v1_pointer_position(void *data, struct river_seat_v1 *obj, int32_t x, int32_t y) {
  Seat *seat = data;
  seat->px = x;
  seat->py = y;

  Output *output;
  wl_list_for_each(output, &anvl.outputs, link) {
    if(x >= output->x && x < output->x + output->width && y >= output->y && y < output->y + output->height) {
      selmon = output;
      river_layer_shell_output_v1_set_default(output->river_layer_shell);
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

void river_layer_shell_output_v1_non_exclusive_area(void *data, struct river_layer_shell_output_v1 *handle, int32_t x, int32_t y, int32_t width, int32_t height) {}

const struct river_layer_shell_output_v1_listener layer_shell_output_listener = {
  .non_exclusive_area = river_layer_shell_output_v1_non_exclusive_area,
};

const struct wl_callback_listener wl_surface_frame_listener;

void manage_seat(Seat *seat) {
  if(seat->focused == NULL && !wl_list_empty(&anvl.windows)) {
    seat->focused = wl_container_of(anvl.windows.prev, seat->focused, link);
  }

  if(seat->focused != NULL) {
    river_seat_v1_focus_window(seat->river_seat, seat->focused->river_window);
    river_node_v1_place_top(seat->focused->river_node);
    seat->focused->node->tag->focused = seat->focused->node;
  } else {
    river_seat_v1_clear_focus(seat->river_seat);
  }
}

void river_window_manager_v1_unavailable(void *data, struct river_window_manager_v1 *obj) {
  fprintf(stderr, "error: Unavailable.\n");
  exit(1);
}

void river_window_manager_v1_finished(void *data, struct river_window_manager_v1 *obj) {
  exit(0);
}

void tile(Output *output) {
  Node *queue[1 << 16];
  uint32_t front = 0;
  uint32_t back = 0;

  queue[back++] = output->tags[output->seltag]->root;

  Node *n;
  while(front != back) {
    n = queue[front++];
    if(n->first != NULL && n->second != NULL) {
      queue[back++] = n->first;
      queue[back++] = n->second;
    } else if(n->window != NULL) {
      river_window_v1_show(n->window->river_window);
      window_set_dimensions(n->window, n->window->node->width, n->window->node->height);
      window_set_position(n->window, n->window->node->x, n->window->node->y);
    }
  }
}

void monocle(Output *output) {
  Node *queue[1 << 16];
  uint32_t front = 0;
  uint32_t back = 0;

  queue[back++] = output->tags[output->seltag]->root;

  Node *n;
  while(front != back) {
    n = queue[front++];
    if(n->first != NULL && n->second != NULL) {
      queue[back++] = n->first;
      queue[back++] = n->second;
    } else if(n->window != NULL) {
      river_window_v1_show(n->window->river_window);
      window_set_position(n->window, output->x, output->y + (show_bar ? barpx : 0));
      window_set_dimensions(n->window, output->width, output->height - (show_bar ? barpx : 0));
    }
  }
}

void river_window_manager_v1_manage_start(void *data, struct river_window_manager_v1 *obj) {
  Seat *seat;
  wl_list_for_each(seat, &anvl.seats, link) {
    manage_seat(seat);
  }

  Window *window;
  wl_list_for_each(window, &anvl.windows, link) {
    river_window_v1_hide(window->river_window);
  }

  Tag *tag;
  Output *output;
  wl_list_for_each(output, &anvl.outputs, link) {
    for(int i = 0; i < LENGTH(output->tags); i++) {
      tag = output->tags[i];
      propogate_layout(tag->root);
    }

    tag = output->tags[output->seltag];
    tag->lt->manage(output);
  }

  river_window_manager_v1_manage_finish(window_manager);
}

// https://wayland-book.com/surfaces/shared-memory.html
// ---
void randname(char *buf) {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  long r = ts.tv_nsec;
  for (int i = 0; i < 6; ++i) {
    buf[i] = 'A'+(r&15)+(r&16)*2;
    r >>= 5;
  }
}

int create_shm_file(void) {
  int retries = 100;
  do {
    char name[] = "/wl_shm-XXXXXX";
    randname(name + sizeof(name) - 7);
    --retries;
    int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
    if(fd >= 0) {
      shm_unlink(name);
      return fd;
    }
  } while (retries > 0 && errno == EEXIST);
  return -1;
}

int allocate_shm_file(size_t size) {
  int fd = create_shm_file();
  if(fd < 0) return -1;

  int ret;
  do {
    ret = ftruncate(fd, size);
  } while (ret < 0 && errno == EINTR);

  if(ret < 0) {
    close(fd);
    return -1;
  }
  return fd;
}
// ---

static pixman_color_t fg = {0xEE00, 0xEE00, 0xEE00, 0xffff};
static pixman_color_t bg = {0x2200, 0x2200, 0x2200, 0xffff};
static pixman_color_t ac = {0x0000, 0x5500, 0x7700, 0xffff};

int lx(int x, int width, int text_width) { return x; }
int cx(int x, int width, int text_width) { return x + (width - text_width) / 2; }
int rx(int x, int width, int text_width) { return x - text_width; }
void render_chars(const char *chars, size_t len, int x, int y, int width, int (*fx)(int, int, int), pixman_image_t *pix, pixman_image_t *color) {
  const struct fcft_glyph *glyphs[len];
  long kern[len];
  int text_width = 0;

  for(size_t i = 0; i < len; i++) {
    glyphs[i] = fcft_rasterize_char_utf32(fcft_font, chars[i], FCFT_SUBPIXEL_DEFAULT);
    if(glyphs[i] == NULL) continue;

    kern[i] = 0;
    if(i > 0) {
      long x_kern;
      if(fcft_kerning(fcft_font, chars[i - 1], chars[i], &x_kern, NULL)) kern[i] = x_kern;
    }

    text_width += kern[i] + glyphs[i]->advance.x;
  }

  int cx = fx(x, width, text_width);
  for(size_t i = 0; i < len; i++) {
    const struct fcft_glyph *g = glyphs[i];
    if(g == NULL) continue;
    cx += kern[i];

    pixman_image_composite32(PIXMAN_OP_OVER, color, g->pix, pix, 0, 0, 0, 0, cx + g->x, y + fcft_font->ascent - g->y, g->width, g->height);

    cx += g->advance.x;
  }
}

void render_bar(WlOutput *output) {
  if(!show_bar) {
    wl_surface_commit(output->surface);
    return;
  }

  if(!output->done) return;
  int w = output->width, h = barpx;

  uint32_t stride = w * 4;
  int shm_pool_size = h * stride;

  int fd = allocate_shm_file(shm_pool_size);
  uint8_t *pool_data = mmap(NULL, shm_pool_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, shm_pool_size);
  struct wl_buffer *buf = wl_shm_pool_create_buffer(pool, 0, w, h, stride, WL_SHM_FORMAT_ARGB8888);

  pixman_image_t *pix = NULL;
  pix = pixman_image_create_bits_no_clear(PIXMAN_a8r8g8b8, w, h, (void*) pool_data, stride);
  if(pix == NULL) {
    fprintf(stderr, "error: failed to create pixman image\n");
    return;
  }

  pixman_image_fill_rectangles(PIXMAN_OP_SRC, pix, &bg, 1, (pixman_rectangle16_t []){{0, 0, w, h}});

  pixman_image_t *color = pixman_image_create_solid_fill(&fg);
  int y = (h - fcft_font->height) / 2;
  int boxw = h + 4; // TODO: remove magic numbers like this

  for(int i = 0; i < LENGTH(tags); i++) {
    if(output->output->seltag == i) {
      pixman_image_fill_rectangles(PIXMAN_OP_SRC, pix, &ac, 1, (pixman_rectangle16_t []){{i*boxw, 0, boxw, h}});
    }

    render_chars(tags[i], 1, i*boxw, y, boxw, &cx, pix, color);
  }

  int ltx = LENGTH(tags)*boxw + boxw / 2;
  render_chars(output->output->tags[output->output->seltag]->lt->symbol, 3, ltx, y, 0, &lx, pix, color);

  time_t rawtime;
  struct tm* timeinfo;
  time(&rawtime);
  timeinfo = localtime(&rawtime);

  char clock[10];
  int l1 = strftime(clock, sizeof(clock), "%H:%M:%S", timeinfo);
  render_chars(clock, l1, 0, y, output->width, &cx, pix, color);

  char date[20];
  int l2 = strftime(date, sizeof(date), "%a, %d %b", timeinfo);
  render_chars(date, l2, output->width - 2, y, 0, &rx, pix, color);

  pixman_image_unref(color);

  wl_surface_attach(output->surface, buf, 0, 0);
  wl_surface_damage(output->surface, 0, 0, w, h);
  wl_surface_commit(output->surface);

  wl_shm_pool_destroy(pool); pool = NULL;
  close(fd); fd = -1;

  pixman_image_unref(pix);
  wl_buffer_destroy(buf);
  munmap(pool_data, shm_pool_size);
}

void river_window_manager_v1_render_start(void *data, struct river_window_manager_v1 *obj) {
  WlOutput *output;
  wl_list_for_each(output, &anvl.wl_outputs, link) {
    render_bar(output);
  }

  river_window_manager_v1_render_finish(window_manager);
}

void wl_surface_frame_done(void *data, struct wl_callback *cb, uint32_t time) {
  wl_callback_destroy(cb);

  WlOutput *output = data;
  cb = wl_surface_frame(output->surface);
  wl_callback_add_listener(cb, &wl_surface_frame_listener, output);

  render_bar(output);
}

const struct wl_callback_listener wl_surface_frame_listener = {
  .done = wl_surface_frame_done,
};

void zwlr_layer_surface_v1_configure(void *data, struct zwlr_layer_surface_v1 *zwlr_layer_surface_v1, uint32_t serial, uint32_t width, uint32_t height) {
  WlOutput *output = data;

  zwlr_layer_surface_v1_ack_configure(zwlr_layer_surface_v1, serial);


  if(output->done && output->width == width && output->height == height) {
      wl_surface_commit(output->surface);
      return;
  }

  output->width = width;
  output->height = height;
  if(output->done) render_bar(output);
}

void zwlr_layer_surface_v1_closed(void *data, struct zwlr_layer_surface_v1 *zwlr_layer_surface_v1) {}

const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
  .configure = zwlr_layer_surface_v1_configure,
  .closed = zwlr_layer_surface_v1_closed,
};

void river_window_manager_v1_session_locked(void *data, struct river_window_manager_v1 *obj) {}
void river_window_manager_v1_session_unlocked(void *data, struct river_window_manager_v1 *obj) {}

void river_window_manager_v1_window(void *data, struct river_window_manager_v1 *obj, struct river_window_v1 *river_window) {
  Window *window = calloc(1, sizeof(Window));
  window->river_window = river_window;
  window->river_node = river_window_v1_get_node(window->river_window);

  Tag *tag = selmon->tags[selmon->seltag]; // TODO: Maybe reconsider using selmon in general
  Node *root = tag->root;
  Node *focused = tag->focused;

  insert_node(window, root, focused);

  river_window_v1_add_listener(window->river_window, &window_listener, window);
  wl_list_insert(&anvl.windows, &window->link);

  river_window_v1_use_ssd(window->river_window);
  river_window_v1_set_tiled(window->river_window, 15);

  window_set_position(window, 0, 0);
  window_set_dimensions(window, 0, 0);

  // Focus new window on all seats
  Seat *seat;
  wl_list_for_each(seat, &anvl.seats, link) {
    seat->focused = window;
  }
}

void river_window_manager_v1_output(void *data, struct river_window_manager_v1 *obj, struct river_output_v1 *river_output) {
  Output *output = calloc(1, sizeof(Output));
  output->river_output = river_output;
  output->river_layer_shell = river_layer_shell_v1_get_output(layer_shell, river_output);
  output->seltag = 0;

  for(int i = 0; i < LENGTH(tags); i++) {
    Tag *tag = calloc(1, sizeof(Tag));
    tag->n = i;
    tag->sym = tags[i];
    tag->root = create_node(tag, NULL, NULL);
    tag->focused = NULL;
    tag->lt = &layouts[0];
    output->tags[i] = tag;
  }

  river_output_v1_add_listener(output->river_output, &output_listener, output);
  river_layer_shell_output_v1_add_listener(output->river_layer_shell, &layer_shell_output_listener, output);
  wl_list_insert(&anvl.outputs, &output->link);

  if(selmon == NULL) {
    selmon = output;
    river_layer_shell_output_v1_set_default(output->river_layer_shell);
  }
}

void river_window_manager_v1_seat(void *data, struct river_window_manager_v1 *obj, struct river_seat_v1 *river_seat) {
  Seat *seat = calloc(1, sizeof(Seat));
  seat->river_seat = river_seat;
  seat->focused = NULL;

  wl_list_init(&seat->keys);
  wl_list_init(&seat->buttons);

  river_seat_v1_add_listener(seat->river_seat, &seat_listener, seat);
  wl_list_insert(&anvl.seats, &seat->link);

  for(int i = 0; i < LENGTH(keybinds); i++) {
    xkb_binding_create(seat, keybinds[i].mods, keybinds[i].key, keybinds[i].func, &keybinds[i].arg);
  }

  for(int i = 0; i < LENGTH(buttons); i++) {
    pointer_binding_create(seat, buttons[i].mods, buttons[i].button, buttons[i].func, &buttons[i].arg);
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
  keymap_rule_names.layout = kb_layout;

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
}

void river_xkb_keymap_v1_failure(void *data, struct river_xkb_keymap_v1 *river_xkb_keymap_v1, const char *error_msg) {
  fprintf(stderr, "Failed to create keymap\n");
}

const struct river_xkb_keymap_v1_listener xkb_keymap_listener = {
  .success = river_xkb_keymap_v1_success,
  .failure = river_xkb_keymap_v1_failure,
};

void wl_output_geometry(void *data, struct wl_output *wl_output, int32_t x, int32_t y, int32_t physical_width, int32_t physical_height,
    int32_t subpixel, const char *make, const char *model, int32_t transform) {}

void wl_output_mode(void *data, struct wl_output *wl_output, uint32_t flags, int32_t width, int32_t height, int32_t refresh) {
  if((flags & WL_OUTPUT_MODE_CURRENT) == 0) return;

  WlOutput *output = data;
  output->width = width;
  output->height = height;
}

void wl_output_done(void *data, struct wl_output *wl_output) {
  WlOutput *output = data;

  output->surface = wl_compositor_create_surface(compositor);

  struct wl_region *input_region = wl_compositor_create_region(compositor);
  wl_surface_set_input_region(output->surface, input_region);
  wl_region_destroy(input_region);

  // Layer set to 1 so fullscreen windows will render above it
  output->layer_surface = zwlr_layer_shell_v1_get_layer_surface(zwlr_layer_shell, output->surface, output->wl_output, 1, "bar");

  zwlr_layer_surface_v1_set_size(output->layer_surface, output->width, 20);
  zwlr_layer_surface_v1_set_anchor(output->layer_surface,
      ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
      ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT |
      ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);
  zwlr_layer_surface_v1_set_exclusive_zone(output->layer_surface, -1);

  zwlr_layer_surface_v1_add_listener(output->layer_surface, &layer_surface_listener, output);
  wl_surface_commit(output->surface);

  struct wl_callback *cb = wl_surface_frame(output->surface);
  wl_callback_add_listener(cb, &wl_surface_frame_listener, output);
}

void wl_output_scale(void *data, struct wl_output *wl_output, int32_t factor) {}
void wl_output_name(void *data, struct wl_output *wl_output, const char *name) {}
void wl_output_description(void *data, struct wl_output *wl_output, const char *description) {}

const struct wl_output_listener wl_output_listener = {
  .geometry = wl_output_geometry,
  .mode = wl_output_mode,
  .done = wl_output_done,
  .scale = wl_output_scale,
  .name = wl_output_name,
  .description = wl_output_description,
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
    input_manager = wl_registry_bind(registry, name, &river_input_manager_v1_interface, 2);
    if(input_manager != NULL) {
      river_input_manager_v1_add_listener(input_manager, &input_manager_listener, NULL);
    }
  }

  if(strcmp(interface, river_xkb_config_v1_interface.name) == 0) {
    xkb_config = wl_registry_bind(registry,name,&river_xkb_config_v1_interface, 1);
    if(xkb_config) {
      river_xkb_config_v1_add_listener(xkb_config, &xkb_config_listener, NULL);

      xkb_keymap = create_keymap();
      if(xkb_keymap) {
        river_xkb_keymap_v1_add_listener(xkb_keymap, &xkb_keymap_listener, NULL);
      }
    }
  }

  if(strcmp(interface, wl_output_interface.name) == 0) {
    WlOutput *output = calloc(1, sizeof(WlOutput));
    output->done = false;
    output->name = name;
    output->wl_output = wl_registry_bind(registry, name, &wl_output_interface, 4);
    wl_output_add_listener(output->wl_output, &wl_output_listener, output);
    wl_list_insert(&anvl.wl_outputs, &output->link);
  }

  if(strcmp(interface, river_layer_shell_v1_interface.name) == 0) {
    layer_shell = wl_registry_bind(registry, name, &river_layer_shell_v1_interface, 1);
  }

  if(strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
    zwlr_layer_shell = wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, 1);
  }

  if(strcmp(interface, wl_compositor_interface.name) == 0) {
    compositor = wl_registry_bind(registry, name, &wl_compositor_interface, version);
  }

  if(strcmp(interface, wl_shm_interface.name) == 0) {
    shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
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

  wl_list_init(&anvl.wl_outputs);
  wl_list_init(&anvl.keyboards);
  wl_list_init(&anvl.windows);
  wl_list_init(&anvl.outputs);
  wl_list_init(&anvl.seats);

  const char *name[] = { font };
  fcft_init(FCFT_LOG_COLORIZE_AUTO, false, FCFT_LOG_CLASS_DEBUG);
  fcft_font = fcft_from_name2(1, name, NULL, NULL);

  if(fcft_font == NULL) {
    fprintf(stderr, "font failed\n");
    return 1;
  }

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

  while(true) {
    if(wl_display_dispatch(display) < 0) {
      fprintf(stderr, "dispatch failed\n");
      return 1;
    }
  }

  fcft_destroy(fcft_font);

  return 0;
}
