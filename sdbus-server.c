#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <systemd/sd-bus.h>
#include <systemd/sd-event.h>
#include <errno.h>
#include <sys/timerfd.h>

#include "sdbus.h"

struct dbusobject {
	char *path;
	char *name;
};

struct manager {
	sd_bus *bus;
	sd_event *event;
};

struct dbusobject *dbusobjects[2] = { NULL, NULL };

/* the interval is in usec, i.e. 10s */
#define TIMER_INTERVAL 10000000

static int
timer_fired(sd_event_source *ev, uint64_t usec, void *userdata)
{
	struct manager *mgr = userdata;
	static unsigned action = 0;
	const char path1[] = "/org/gnome/TestCase/foo";
	const char path2[] = "/org/gnome/TestCase/bar";
	const char *path;

	if (action % 2)
		path = path1;
	else
		path = path2;

	sd_event_source_set_time(ev, usec + TIMER_INTERVAL);
	sd_event_source_set_enabled(ev, SD_EVENT_ONESHOT);

	if (action < 2) {
		struct dbusobject *dbo;
	       
		if (dbusobjects[action]) {
			fprintf(stderr, "Weird, object already there!?\n");
			return 0;
		}
	
		dbo = malloc(sizeof(*dbo));
		if (!dbo) {
			fprintf(stderr, "malloc failed\n");
			return 0;
		}

		dbo->path = strdup(path);
		if (!dbo->path) {
			fprintf(stderr, "strdup failed\n");
			free(dbo);
			return 0;
		}
		dbo->name = dbo->path + strlen("/org/gnome/TestCase/");
		dbusobjects[action] = dbo;
		fprintf(stderr, "Added object at path %s\n", dbo->path);
		sd_bus_emit_object_added(mgr->bus, dbo->path);

	} else {
		struct dbusobject *dbo = dbusobjects[action - 2];

		dbusobjects[action - 2] = NULL;
		if (!dbo) {
			fprintf(stderr, "Weird, object missing?!\n");
			return 0;
		}

		sd_bus_emit_object_removed(mgr->bus, dbo->path);
		fprintf(stderr, "Removed object at path %s\n", dbo->path);
		free(dbo->path);
		free(dbo);
	}

	action++;
	if (action > 3)
		action = 0;	
	return 0;
}

static int
property_get(sd_bus *bus, const char *path, const char *interface,
	     const char *property, sd_bus_message *reply, void *userdata,
	     sd_bus_error *error)
{

	for (int i = 0; i < ARRAY_SIZE(dbusobjects); i++) {
		struct dbusobject *dbo = dbusobjects[i];

		if (!dbo)
			continue;

		if (strcmp(dbo->path, path))
			continue;

		if (!strcmp(property, "Name")) {
			/* sd-bus calls this method internally so no debug msg here, too chatty */
			return sd_bus_message_append(reply, "s", dbo->name);
		}

		fprintf(stderr, "property_get called for path %s, invalid property %s\n",
			path, property);
		sd_bus_error_set_const(error, "org.gnome.TestCase.InvalidProperty", "Sorry, invalid property");
		return -EINVAL;
	}

	fprintf(stderr, "property_get called for invalid path %s, property %s\n",
		path, property);
	sd_bus_error_set_const(error, "org.gnome.TestCase.InvalidPath", "Sorry, invalid path");
	return -EINVAL;
}

static const sd_bus_vtable device_vtable[] = {
	SD_BUS_VTABLE_START(0),
	SD_BUS_PROPERTY("Name", "s", property_get, 0, 0),
	SD_BUS_VTABLE_END
};

static int
enumerator(sd_bus *bus, const char *path, void *userdata, char ***retnodes, sd_bus_error *error)
{
	_cleanup_strv_free_ char **nodes = NULL;
	unsigned offset = 0;

	if (!path)
		return 0;

	nodes = zmalloc((ARRAY_SIZE(dbusobjects) + 1) * sizeof(char *));
	if (!nodes) {
		fprintf(stderr, "Faled to malloc array\n");
		return -ENOMEM;
	}

	for (int i = 0; i < ARRAY_SIZE(dbusobjects); i++) {
		if (!dbusobjects[i])
			continue;
		nodes[offset] = strdup(dbusobjects[i]->path);
		if (!nodes[offset]) {
			fprintf(stderr, "Failed to strdup object path\n");
			return -ENOMEM;
		}
		offset++;
	}

	fprintf(stderr, "enumerator called for path %s, found %u objects\n", path, offset);
	nodes[offset++] = NULL;
	*retnodes = nodes;
	nodes = NULL;

	return 1;
}

static void
free_manager(struct manager *mgr) {
	if (!mgr)
		return;

	sd_bus_detach_event(mgr->bus);
	sd_event_unref(mgr->event);
	free(mgr);
}

static int
block_signals(void)
{
	sigset_t sigset;
	int r;

	r = sigemptyset(&sigset);
	if (r < 0)
		return r;

	r = sigaddset(&sigset, SIGINT);
	if (r < 0)
		return r;

	r = sigaddset(&sigset, SIGTERM);
	if (r < 0)
		return r;

	return sigprocmask(SIG_BLOCK, &sigset, NULL);
}

int
main(int argc, char **argv)
{
	int r;
	struct manager *mgr;
	_cleanup_bus_flush_close_unref_ sd_bus *bus = NULL;
	_cleanup_bus_slot_unref_ struct sd_bus_slot *vtable_slot = NULL;
	_cleanup_bus_slot_unref_ struct sd_bus_slot *enumerator_slot = NULL;
	_cleanup_bus_slot_unref_ struct sd_bus_slot *objm_slot = NULL;
	_cleanup_event_source_unref_ sd_event_source *sigint_ev = NULL;
	_cleanup_event_source_unref_ sd_event_source *sigterm_ev = NULL;
	_cleanup_event_source_unref_ sd_event_source *timer_ev = NULL;
	uint64_t ev_time;

	mgr = zmalloc(sizeof(*mgr));
	if (!mgr) {
		fprintf(stderr, "Failed to allocate memory: %m\n");
		goto finish;
	}

	r = sd_event_default(&mgr->event);
	if (r < 0) {
		fprintf(stderr, "Failed to create sd_event: %s\n", strerror(-r));
		goto finish;
	}

	r = sd_bus_open_user(&bus);
	if (r < 0) {
		fprintf(stderr, "Failed to connect to system bus: %s\n", strerror(-r));
		goto finish;
	}
	mgr->bus = bus;

	r = sd_bus_request_name(mgr->bus, "org.gnome.TestCase", 0);
	if (r < 0) {
		fprintf(stderr, "Failed to acquire service name: %s\n", strerror(-r));
		goto finish;
	}

	r = sd_bus_add_fallback_vtable(mgr->bus, &vtable_slot, "/org/gnome/TestCase",
				       "org.gnome.TestCase.TestDevice",
				       device_vtable, NULL, mgr);
	if (r < 0) {
		fprintf(stderr, "Failed to add vtable: %s\n", strerror(-r));
		goto finish;
	}

	r = sd_bus_add_node_enumerator(mgr->bus, &enumerator_slot, "/org/gnome/TestCase", enumerator, mgr);
	if (r < 0) {
		fprintf(stderr, "Failed to create node enumerator: %s\n", strerror(-r));
		goto finish;
	}

	r = sd_bus_add_object_manager(mgr->bus, &objm_slot, "/org/gnome/TestCase");
	if (r < 0) {
		fprintf(stderr, "Failed to create object manager: %s\n", strerror(-r));
		goto finish;
	}

	r = sd_bus_attach_event(mgr->bus, mgr->event, 0);
	if (r < 0) {
		fprintf(stderr, "Failed to attach bus to event loop: %s\n", strerror(-r));
		goto finish;
	}

	r = block_signals();
	if (r < 0) {
		fprintf(stderr, "Failed to block signals: %m\n");
		goto finish;
	}

	sd_event_add_signal(mgr->event, &sigint_ev, SIGINT, NULL, NULL);
	sd_event_add_signal(mgr->event, &sigterm_ev, SIGTERM, NULL, NULL);

	sd_event_now(mgr->event, CLOCK_MONOTONIC, &ev_time);
	sd_event_add_time(mgr->event, &timer_ev, CLOCK_MONOTONIC, ev_time + 10000000, 0, timer_fired, mgr);

	fprintf(stderr, "Server running\n");
	r = sd_event_loop(mgr->event);
	if (r < 0) {
		fprintf(stderr, "Event loop failed: %s\n", strerror(-r));
		goto finish;
	}

	sd_event_get_exit_code(mgr->event, &r);

finish:
	fprintf(stderr, "Server exiting\n");
	free_manager(mgr);

	return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}

