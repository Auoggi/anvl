#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include <wayland-client-core.h>
#include <wayland-client-protocol.h>

#include <linux/input-event-codes.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include <river-xkb-bindings-v1-client-protocol.h>
#include <river-window-management-v1-client-protocol.h>

struct river_window_manager_v1 *window_manager;
struct river_xkb_bindings_v1 *xkb_bindings;

void river_output_v1_removed(void *data, struct river_output_v1 *obj) {}
void river_output_v1_wl_output(void *data, struct river_output_v1 *obj, uint32_t name) {}
void river_output_v1_position(void *data, struct river_output_v1 *obj, int32_t x, int32_t y) {}
void river_output_v1_dimensions(void *data, struct river_output_v1 *obj, int32_t width, int32_t height) {}

void river_window_v1_closed(void *data, struct river_window_v1 *obj) {}
void river_window_v1_dimensions_hint(void *data, struct river_window_v1 *obj, int32_t min_width, int32_t min_height, int32_t max_width, int32_t max_height) {}
void river_window_v1_dimensions(void *data, struct river_window_v1 *obj, int32_t width, int32_t height) {}
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

void river_xkb_binding_v1_pressed(void *data, struct river_xkb_binding_v1 *obj) {}
void river_xkb_binding_v1_released(void *data, struct river_xkb_binding_v1 *obj) {}

void river_pointer_binding_v1_pressed(void *data, struct river_pointer_binding_v1 *obj) {}
void river_pointer_binding_v1_released(void *data, struct river_pointer_binding_v1 *obj) {}

void river_seat_v1_removed(void *data, struct river_seat_v1 *obj) {}
void river_seat_v1_wl_seat(void *data, struct river_seat_v1 *obj, uint32_t id) {}
void river_seat_v1_pointer_enter(void *data, struct river_seat_v1 *obj, struct river_window_v1 *river_window) {}
void river_seat_v1_pointer_leave(void *data, struct river_seat_v1 *obj) {}
void river_seat_v1_window_interaction(void *data, struct river_seat_v1 *obj, struct river_window_v1 *river_window) {}
void river_seat_v1_shell_surface_interaction(void *data, struct river_seat_v1 *obj, struct river_shell_surface_v1 *river_shell_surface) {}
void river_seat_v1_op_delta(void *data, struct river_seat_v1 *obj, int32_t dx, int32_t dy) {}
void river_seat_v1_op_release(void *data, struct river_seat_v1 *obj) {}
void river_seat_v1_pointer_position(void *data, struct river_seat_v1 *obj, int32_t x, int32_t y) {}

void river_window_manager_v1_unavailable(void *data, struct river_window_manager_v1 *obj) {}
void river_window_manager_v1_finished(void *data, struct river_window_manager_v1 *obj) {}
void river_window_manager_v1_manage_start(void *data, struct river_window_manager_v1 *obj) {}
void river_window_manager_v1_render_start(void *data, struct river_window_manager_v1 *obj) {}
void river_window_manager_v1_session_locked(void *data, struct river_window_manager_v1 *obj) {}
void river_window_manager_v1_session_unlocked(void *data, struct river_window_manager_v1 *obj) {}
void river_window_manager_v1_window(void *data, struct river_window_manager_v1 *obj, struct river_window_v1 *river_window) {}
void river_window_manager_v1_output(void *data, struct river_window_manager_v1 *obj, struct river_output_v1 *river_output) {}
void river_window_manager_v1_seat(void *data, struct river_window_manager_v1 *obj, struct river_seat_v1 *river_seat) {}

const struct river_output_v1_listener output_listener = {
	.removed = river_output_v1_removed,
	.wl_output = river_output_v1_wl_output,
	.position = river_output_v1_position,
	.dimensions = river_output_v1_dimensions,
};

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

const struct river_xkb_binding_v1_listener xkb_binding_listener = {
	.pressed = river_xkb_binding_v1_pressed,
	.released = river_xkb_binding_v1_released,
};

const struct river_pointer_binding_v1_listener pointer_binding_listener = {
	.pressed = river_pointer_binding_v1_pressed,
	.released = river_pointer_binding_v1_released,
};

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

static const struct river_window_manager_v1_listener window_manager_listener = {
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

static void wl_registry_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
	if(strcmp(interface, river_window_manager_v1_interface.name) == 0 && version >= 4) {
    window_manager = wl_registry_bind(registry, name, &river_window_manager_v1_interface, 4);
	}

  if(strcmp(interface, river_xkb_bindings_v1_interface.name) == 0) {
		xkb_bindings = wl_registry_bind(registry, name, &river_xkb_bindings_v1_interface, 1);
	}
}

static void wl_registry_global_remove(void *data, struct wl_registry *registry, uint32_t name) {}

static const struct wl_registry_listener registry_listener = {
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

	river_window_manager_v1_add_listener(window_manager, &window_manager_listener, NULL);

	while(true) {
		if(wl_display_dispatch(display) < 0) {
			fprintf(stderr, "dispatch failed\n");
			return 1;
		}
	}

	return 0;
}
