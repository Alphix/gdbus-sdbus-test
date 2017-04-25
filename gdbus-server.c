#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <gtk/gtk.h>

#include "generated.h"


static gboolean
my_timer(gpointer user_data)
{
	GDBusObjectManagerServer *manager = user_data;
	static guint add = 0;
	gchar *s1 = "/org/gnome/TestCase/foo";
	gchar *s2 = "/org/gnome/TestCase/bar";
	gchar *path;

	if (add % 2 == 0)
		path = s1;
	else
		path = s2;

	if (add < 2) {
		TestDevice *td;
		ObjectSkeleton *object;

		g_print("Adding object: %s\n", path);
		object = object_skeleton_new(path);
		td = test_device_skeleton_new();
		test_device_set_name(td, path + strlen("/org/gnome/TestCase/"));
		object_skeleton_set_test_device(object, td);
		g_object_unref(td);

		g_dbus_object_manager_server_export(manager, G_DBUS_OBJECT_SKELETON(object));
		g_object_unref(object);

	} else {
		g_print("Removing object: %s\n", path);
		g_dbus_object_manager_server_unexport(manager, path);
	}

	add++;
	if (add > 3)
		add = 0;
	return TRUE;
}

static void
on_bus_acquired (GDBusConnection *connection,
                 const gchar     *name,
                 gpointer         user_data)
{
	GDBusObjectManagerServer *manager;

	g_print("Acquired a message bus connection\n");

	manager = g_dbus_object_manager_server_new("/org/gnome/TestCase");

	g_dbus_object_manager_server_set_connection(manager, connection);

	g_timeout_add_seconds(10, my_timer, manager);
}

static void
on_name_acquired (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
	g_print ("Acquired the name %s\n", name);
}

static void
on_name_lost (GDBusConnection *connection,
              const gchar     *name,
              gpointer         user_data)
{
	g_print("Lost the name %s\n", name);
}

gint
main(gint argc, gchar *argv[])
{
	GMainLoop *loop;
	guint id;

	loop = g_main_loop_new(NULL, FALSE);

	id = g_bus_own_name(G_BUS_TYPE_SESSION,
			    "org.gnome.TestCase",
			    G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT |
			    G_BUS_NAME_OWNER_FLAGS_REPLACE,
			    on_bus_acquired,
			    on_name_acquired,
			    on_name_lost,
			    loop,
			    NULL);

	g_main_loop_run(loop);
	g_bus_unown_name(id);
	g_main_loop_unref(loop);

	return 0;
}
