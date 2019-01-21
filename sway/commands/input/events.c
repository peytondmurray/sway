#include <limits.h>
#include <string.h>
#include <strings.h>
#include <wlr/backend/libinput.h>
#include "sway/config.h"
#include "sway/commands.h"
#include "sway/input/input-manager.h"
#include "log.h"

static void toggle_supported_send_events_for_device(struct input_config *ic,
		struct sway_input_device *input_device) {
	struct wlr_input_device *wlr_device = input_device->wlr_device;
	if (!wlr_input_device_is_libinput(wlr_device)) {
		return;
	}
	struct libinput_device *libinput_dev =
		wlr_libinput_get_device_handle(wlr_device);

	enum libinput_config_send_events_mode mode =
		libinput_device_config_send_events_get_mode(libinput_dev);
	uint32_t possible =
		libinput_device_config_send_events_get_modes(libinput_dev);

	switch (mode) {
	case LIBINPUT_CONFIG_SEND_EVENTS_ENABLED:
		mode = LIBINPUT_CONFIG_SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE;
		if (possible & mode) {
			break;
		}
		// fall through
	case LIBINPUT_CONFIG_SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE:
		mode = LIBINPUT_CONFIG_SEND_EVENTS_DISABLED;
		if (possible & mode) {
			break;
		}
		// fall through
	case LIBINPUT_CONFIG_SEND_EVENTS_DISABLED:
	default:
		mode = LIBINPUT_CONFIG_SEND_EVENTS_ENABLED;
		break;
	}

	ic->send_events = mode;
}

static int mode_for_name(const char *name) {
	if (!strcmp(name, "enabled")) {
		return LIBINPUT_CONFIG_SEND_EVENTS_ENABLED;
	} else if (!strcmp(name, "disabled_on_external_mouse")) {
		return LIBINPUT_CONFIG_SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE;
	} else if (!strcmp(name, "disabled")) {
		return LIBINPUT_CONFIG_SEND_EVENTS_DISABLED;
	}
	return -1;
}

static void toggle_select_send_events_for_device(struct input_config *ic,
		struct sway_input_device *input_device, int argc, char **argv) {
	if (!wlr_input_device_is_libinput(input_device->wlr_device)) {
		return;
	}
	// Get the currently set event mode since ic is a new config that will be
	// merged on the existing later. It should be set to INT_MIN before this.
	ic->send_events = libinput_device_config_send_events_get_mode(
			wlr_libinput_get_device_handle(input_device->wlr_device));

	int index;
	for (index = 0; index < argc; ++index) {
		if (mode_for_name(argv[index]) == ic->send_events) {
			++index;
			break;
		}
	}
	ic->send_events = mode_for_name(argv[index % argc]);
}

static void toggle_send_events(struct input_config *ic, int argc, char **argv) {
	struct sway_input_device *input_device = NULL;
	wl_list_for_each(input_device, &server.input->devices, link) {
		if (strcmp(input_device->identifier, ic->identifier) == 0) {
			if (argc) {
				toggle_select_send_events_for_device(ic, input_device,
						argc, argv);
			} else {
				toggle_supported_send_events_for_device(ic, input_device);
			}
			return;
		}
	}
}

static void toggle_wildcard_send_events(int argc, char **argv) {
	struct sway_input_device *input_device = NULL;
	wl_list_for_each(input_device, &server.input->devices, link) {
		struct input_config *ic = new_input_config(input_device->identifier);
		if (!ic) {
			break;
		}
		if (argc) {
			toggle_select_send_events_for_device(ic, input_device, argc, argv);
		} else {
			toggle_supported_send_events_for_device(ic, input_device);
		}
		store_input_config(ic);
	}
}

struct cmd_results *input_cmd_events(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "events", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	struct input_config *ic = config->handler_context.input_config;
	if (!ic) {
		return cmd_results_new(CMD_FAILURE, "No input device defined.");
	}

	if (strcasecmp(argv[0], "enabled") == 0) {
		ic->send_events = LIBINPUT_CONFIG_SEND_EVENTS_ENABLED;
	} else if (strcasecmp(argv[0], "disabled") == 0) {
		ic->send_events = LIBINPUT_CONFIG_SEND_EVENTS_DISABLED;
	} else if (strcasecmp(argv[0], "disabled_on_external_mouse") == 0) {
		ic->send_events =
			LIBINPUT_CONFIG_SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE;
	} else if (config->reading) {
		return cmd_results_new(CMD_INVALID,
			"Expected 'events <enabled|disabled|disabled_on_external_mouse>'");
	} else if (strcasecmp(argv[0], "toggle") == 0) {
		for (int i = 1; i < argc; ++i) {
			if (mode_for_name(argv[i]) == -1) {
				return cmd_results_new(CMD_INVALID,
						"Invalid toggle mode %s", argv[i]);
			}
		}
		if (strcmp(ic->identifier, "*") == 0) {
			// Update the device input configs and then reset the wildcard
			// config send events mode so that is does not override the device
			// ones. The device ones will be applied when attempting to apply
			// the wildcard config
			toggle_wildcard_send_events(argc - 1, argv + 1);
			ic->send_events = INT_MIN;
		} else {
			toggle_send_events(ic, argc - 1, argv + 1);
		}
	} else {
		return cmd_results_new(CMD_INVALID,
			"Expected 'events <enabled|disabled|disabled_on_external_mouse|"
			"toggle>'");
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}
