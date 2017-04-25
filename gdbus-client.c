#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>

#include "generated.h"

void
dump_objects(GDBusObjectManager *manager)
{
	GList *objects;
	GList *l;

	g_print("* Dumping objects\n");
	objects = g_dbus_object_manager_get_objects(manager);
	for (l = objects; l != NULL; l = l->next) {
		GList *interfaces;
		GDBusInterface *interface;
		GList *ll;

		g_print(" - Object at %s\n", g_dbus_object_get_object_path(G_DBUS_OBJECT(l->data)));

		interfaces = g_dbus_object_get_interfaces(G_DBUS_OBJECT(l->data));
		for (ll = interfaces; ll != NULL; ll = ll->next) {
			GDBusInterface *interface = G_DBUS_INTERFACE(ll->data);
			g_print("   - Interface %s\n",
				g_dbus_interface_get_info(interface) ? 
				g_dbus_interface_get_info(interface)->name :
				"<NONAME!?>");
		}
		g_list_free_full (interfaces, g_object_unref);

		interface = g_dbus_object_get_interface(G_DBUS_OBJECT(l->data),
							"org.gnome.TestCase.TestDevice");
		if (!interface) {
			g_print("That's weird, an object which is not a TestDevice!?...\n");
			continue;
		}

		TestDevice *td = TEST_DEVICE(interface);
		printf("   - Name is: %s\n", test_device_get_name(td));

		g_object_unref(interface);
	}
	g_list_free_full (objects, g_object_unref);
	g_print("* Done dumping objects\n");
}

static void
on_object_added(GDBusObjectManager *manager,
		GDBusObject        *object,
		gpointer            user_data)
{
	g_print("Added object at %s\n", g_dbus_object_get_object_path(object));
	dump_objects(manager);
}

static void
on_object_removed(GDBusObjectManager *manager,
		  GDBusObject        *object,
		  gpointer            user_data)
{
	g_print("Removed object at %s\n", g_dbus_object_get_object_path(object));
	dump_objects(manager);
}

static void
on_signal(GDBusObjectManagerClient *manager,
	  GDBusObjectProxy         *object_proxy,
	  GDBusProxy               *interface_proxy,
	  gchar                    *sender_name,
	  gchar                    *signal_name,
	  GVariant                 *parameters,
	  gpointer                  user_data)
{
	g_print("Signal %s received for obj %s\n", signal_name,
		g_dbus_object_get_object_path(G_DBUS_OBJECT(object_proxy)));
}

static void
on_owner_change(GDBusObjectManager *manager, gpointer user_data)
{
	gchar *owner;

	owner = g_dbus_object_manager_client_get_name_owner(G_DBUS_OBJECT_MANAGER_CLIENT(manager));

	if (owner)
		g_print("Server available at %s\n", owner);
	else
		g_print("Server gone\n");
}

static void
manager_ready_cb(GObject *source, GAsyncResult *res, gpointer user_data)
{
	GDBusObjectManager *manager;
	GError *error = NULL;

	manager = object_manager_client_new_for_bus_finish(res, &error);
	if (!manager) {
		g_print("Error creating object manager client: %s",
			error->message);
		g_error_free(error);
		return;
	}

	g_signal_connect(manager, "object-added", G_CALLBACK(on_object_added), NULL);
	g_signal_connect(manager, "object-removed", G_CALLBACK(on_object_removed), NULL);
	g_signal_connect(manager, "interface-proxy-signal", G_CALLBACK(on_signal), NULL);
	g_signal_connect(manager, "notify::name-owner", G_CALLBACK(on_owner_change), NULL);

	g_print("Object manager client for %s ready\n", g_dbus_object_manager_get_object_path(manager));
	dump_objects(manager);
}

int
main(int argc, char *argv[])
{
	GMainLoop *loop;

	loop = g_main_loop_new(NULL, FALSE);

	object_manager_client_new_for_bus(G_BUS_TYPE_SESSION,
					  G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE,
					  "org.gnome.TestCase",
					  "/org/gnome/TestCase",
					  NULL, manager_ready_cb, NULL);

	g_main_loop_run(loop);
	g_main_loop_unref(loop);

	return 0;
}
