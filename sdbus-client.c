#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <systemd/sd-bus.h>

#include "sdbus.h"

static const char *
bus_error_message(const sd_bus_error *e, int error) {

	if (e) {
		/* Sometimes, the D-Bus server is a little bit too verbose with
		 * its error messages, so let's override them here */
		if (sd_bus_error_has_name(e, SD_BUS_ERROR_ACCESS_DENIED))
			return "Access denied";

		if (e->message)
			return e->message;
	}

	if (error < 0)
		error = -error;

	return strerror(error);
}

int
main(int argc, char **argv)
{
	int r;
	_cleanup_bus_flush_close_unref_ sd_bus *bus = NULL;
	_cleanup_bus_error_free_ sd_bus_error error = SD_BUS_ERROR_NULL;
	_cleanup_bus_message_unref_ sd_bus_message *m = NULL;
	char type;
	const char *contents;

	r = sd_bus_open_user(&bus);
	if (r < 0) {
		fprintf(stderr, "Failed to connect to system bus: %s\n", strerror(-r));
		goto finish;
	}

	r = sd_bus_call_method(bus,
			       "org.gnome.TestCase",
			       "/org/gnome/TestCase",
			       "org.freedesktop.DBus.ObjectManager",
			       "GetManagedObjects",
			       &error,
			       &m,
			       NULL);
	if (r < 0) {
		fprintf(stderr, "Could not get object list: %s\n", bus_error_message(&error, -r));
		return r;
	}

	fprintf(stderr, "Dumping objects\n");
	r = sd_bus_message_enter_container(m, SD_BUS_TYPE_ARRAY, "{oa{sa{sv}}}");
	if (r < 0)
		goto finish;

	while ((r = sd_bus_message_enter_container(m, SD_BUS_TYPE_DICT_ENTRY, "oa{sa{sv}}")) > 0) {
		const char *path;

		r = sd_bus_message_read_basic(m, SD_BUS_TYPE_OBJECT_PATH, &path);
		if (r < 0)
			return r;

		fprintf(stderr, "Object path: %s\n", path);

		r = sd_bus_message_enter_container(m, SD_BUS_TYPE_ARRAY, "{sa{sv}}");
		if (r < 0)
			goto finish;

		while ((r = sd_bus_message_enter_container(m, SD_BUS_TYPE_DICT_ENTRY, "sa{sv}")) > 0) {
			const char *iface;

			r = sd_bus_message_read_basic(m, SD_BUS_TYPE_STRING, &iface);
			if (r < 0)
				return r;

			fprintf(stderr, "  - Interface: %s\n", iface);

			r = sd_bus_message_enter_container(m, SD_BUS_TYPE_ARRAY, "{sv}");
			if (r < 0)
				goto finish;

			while ((r = sd_bus_message_enter_container(m, SD_BUS_TYPE_DICT_ENTRY, "sv")) > 0) {
				const char *name;
				const char *value;

				r = sd_bus_message_read_basic(m, SD_BUS_TYPE_STRING, &name);
				if (r < 0)
					return r;

				r = sd_bus_message_peek_type(m, NULL, &contents);
				if (r < 0)
					return r;

				r = sd_bus_message_enter_container(m, SD_BUS_TYPE_VARIANT, contents);
				if (r < 0)
					return r;

				r = sd_bus_message_peek_type(m, &type, &contents);
				if (r < 0)
					return r;

				if (type != SD_BUS_TYPE_STRING) {
					fprintf(stderr, "Invalid type for parameter (%c)\n", type);
					return -EINVAL;
				}

				r = sd_bus_message_read_basic(m, SD_BUS_TYPE_STRING, &value);
				if (r < 0)
					return r;
				fprintf(stderr, "    - Property %s = %s\n", name, value);

				r = sd_bus_message_exit_container(m);
				if (r < 0)
					return r;

				r = sd_bus_message_exit_container(m);
				if (r < 0)
					return r;
			}

			r = sd_bus_message_exit_container(m);
			if (r < 0)
				return r;

			r = sd_bus_message_exit_container(m);
			if (r < 0)
				return r;
		}

		r = sd_bus_message_exit_container(m);
		if (r < 0)
			return r;

		r = sd_bus_message_exit_container(m);
		if (r < 0)
			return r;
	}
	fprintf(stderr, "Done dumping objects\n");

finish:
	return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}

