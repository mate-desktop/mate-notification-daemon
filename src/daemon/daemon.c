/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2006 Christian Hammond <chipx86@chipx86.com>
 * Copyright (C) 2005 John (J5) Palmieri <johnp@redhat.com>
 * Copyright (C) 2010 Red Hat, Inc.
 * Copyright (C) 2011 Perberos <perberos@gmail.com>
 * Copyright (C) 2012-2021 MATE Developers
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include "config.h"

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#include <glib/gi18n.h>
#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#ifdef HAVE_X11
#include <X11/Xproto.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <gdk/gdkx.h>

#define WNCK_I_KNOW_THIS_IS_UNSTABLE
#include <libwnck/libwnck.h>
#endif /* HAVE_X11 */

#include "daemon.h"
#include "engines.h"
#include "stack.h"
#include "sound.h"
#include "mnd-dbus-generated.h"

#define MAX_NOTIFICATIONS 20
#define IMAGE_SIZE 48
#define IDLE_SECONDS 30

#define NW_GET_NOTIFY_ID(nw) \
	(GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(nw), "_notify_id")))
#define NW_GET_NOTIFY_SENDER(nw) \
	(g_object_get_data(G_OBJECT(nw), "_notify_sender"))
#define NW_GET_DAEMON(nw) \
	(g_object_get_data(G_OBJECT(nw), "_notify_daemon"))

enum {
	PROP_0,
	PROP_REPLACE,
	LAST_PROP
};

typedef struct {
	NotifyStackLocation type;
	const char* identifier;
} PopupNotifyStackLocation;

const PopupNotifyStackLocation popup_stack_locations[] = {
	{NOTIFY_STACK_LOCATION_TOP_LEFT, "top_left"},
	{NOTIFY_STACK_LOCATION_TOP_RIGHT, "top_right"},
	{NOTIFY_STACK_LOCATION_BOTTOM_LEFT, "bottom_left"},
	{NOTIFY_STACK_LOCATION_BOTTOM_RIGHT, "bottom_right"},
	{NOTIFY_STACK_LOCATION_UNKNOWN, NULL}
};

#define POPUP_STACK_DEFAULT_INDEX 3     /* XXX Hack! */

typedef struct {
	NotifyDaemon *daemon;
	gint64        expiration;
	GTimeSpan     paused_diff;
	guint         id;
	GtkWindow    *nw;
	guint         has_timeout : 1;
	guint         paused : 1;
#ifdef HAVE_X11
	Window        src_window_xid;
#endif /* HAVE_X11 */
} NotifyTimeout;

typedef struct {
	NotifyStack** stacks;
	gsize n_stacks;
#ifdef HAVE_X11
	Atom workarea_atom;
#endif /* HAVE_X11 */
} NotifyScreen;

struct _NotifyDaemon {
	GObject parent;
	GSettings* gsettings;
	guint next_id;
	guint timeout_source;
	guint exit_timeout_source;
	GHashTable* idle_reposition_notify_ids;
	GHashTable* monitored_window_hash;
	GHashTable* notification_hash;
	gboolean url_clicked_lock;

	NotifyDaemonNotifications *skeleton;
	guint              bus_name_id;
	gboolean           replace;

	NotifyStackLocation stack_location;
	NotifyScreen* screen;
};

typedef struct {
	guint id;
	NotifyDaemon* daemon;
} _NotifyPendingClose;

static void notify_daemon_finalize(GObject* object);
static void _notification_destroyed_cb(GtkWindow* nw, NotifyDaemon* daemon);
static void _close_notification(NotifyDaemon* daemon, guint id, gboolean hide_notification, NotifydClosedReason reason);
static void _emit_closed_signal(GtkWindow* nw, NotifydClosedReason reason);
static void _action_invoked_cb(GtkWindow* nw, const char* key);
static NotifyStackLocation get_stack_location_from_string(const gchar *slocation);

#ifdef HAVE_X11
static GdkFilterReturn _notify_x11_filter(GdkXEvent* xevent, GdkEvent* event, NotifyDaemon* daemon);
static void sync_notification_position(NotifyDaemon* daemon, GtkWindow* nw, Window source);
static void monitor_notification_source_windows(NotifyDaemon* daemon, NotifyTimeout* nt, Window source);
#endif /* HAVE_X11 */

static gboolean notify_daemon_notify_handler(NotifyDaemonNotifications *object, GDBusMethodInvocation *invocation, const gchar *app_name, guint id, const gchar *icon, const gchar *summary, const gchar *body, const gchar *const *actions, GVariant *hints, gint timeout, gpointer user_data);
static gboolean notify_daemon_close_notification_handler(NotifyDaemonNotifications *object, GDBusMethodInvocation *invocation, guint arg_id, gpointer user_data);
static gboolean notify_daemon_get_capabilities( NotifyDaemonNotifications *object, GDBusMethodInvocation *invocation);
static gboolean notify_daemon_get_server_information (NotifyDaemonNotifications *object, GDBusMethodInvocation *invocation, gpointer user_data);

static GParamSpec *properties[LAST_PROP] = { NULL };

G_DEFINE_TYPE(NotifyDaemon, notify_daemon, G_TYPE_OBJECT);

static void bus_acquired_handler_cb (GDBusConnection *connection,
		const gchar     *name,
		gpointer         user_data)
{
	NotifyDaemon *daemon;

	GError *error = NULL;
	gboolean exported;

	daemon = NOTIFY_DAEMON (user_data);

	g_signal_connect (daemon->skeleton, "handle-notify", G_CALLBACK (notify_daemon_notify_handler), daemon);
	g_signal_connect (daemon->skeleton, "handle-close-notification", G_CALLBACK (notify_daemon_close_notification_handler), daemon);
	g_signal_connect (daemon->skeleton, "handle-get-capabilities", G_CALLBACK (notify_daemon_get_capabilities), daemon);
	g_signal_connect (daemon->skeleton, "handle-get-server-information", G_CALLBACK (notify_daemon_get_server_information), daemon);

	exported = g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (daemon->skeleton),
			connection, NOTIFICATION_BUS_PATH, &error);
	if (!exported)
	{
		g_warning ("Failed to export interface: %s", error->message);
		g_error_free (error);

		gtk_main_quit();
	}
}

static void name_lost_handler_cb (GDBusConnection *connection,
		const gchar     *name,
		gpointer         user_data)
{
	g_debug("bus name lost\n");
	gtk_main_quit();
}

static void notify_daemon_constructed (GObject *object)
{
	NotifyDaemon *daemon;

	GBusNameOwnerFlags flags;

	daemon = NOTIFY_DAEMON (object);

	G_OBJECT_CLASS (notify_daemon_parent_class)->constructed (object);

	flags = G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT;
	if (daemon->replace)
		flags |= G_BUS_NAME_OWNER_FLAGS_REPLACE;

	daemon->bus_name_id = g_bus_own_name (G_BUS_TYPE_SESSION,
			NOTIFICATION_BUS_NAME, flags,
			bus_acquired_handler_cb, NULL,
			name_lost_handler_cb, daemon, NULL);
}

static void notify_daemon_set_property (GObject      *object,
		guint         prop_id,
		const GValue *value,
		GParamSpec   *pspec)
{
	NotifyDaemon *daemon;

	daemon = NOTIFY_DAEMON (object);

	switch (prop_id)
	{
		case PROP_REPLACE:
			daemon->replace = g_value_get_boolean (value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void notify_daemon_class_init(NotifyDaemonClass* daemon_class)
{
	GObjectClass* object_class = G_OBJECT_CLASS(daemon_class);
	object_class->set_property = notify_daemon_set_property;

	object_class->constructed = notify_daemon_constructed;
	object_class->finalize = notify_daemon_finalize;
	properties[PROP_REPLACE] =
		g_param_spec_boolean ("replace", "replace", "replace", FALSE,
				G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE |
				G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void _notify_timeout_destroy(NotifyTimeout* nt)
{
	/*
	 * Disconnect the destroy handler to avoid a loop since the id
	 * won't be removed from the hash table before the widget is
	 * destroyed.
	 */
	g_signal_handlers_disconnect_by_func(nt->nw, _notification_destroyed_cb, nt->daemon);
	gtk_widget_destroy(GTK_WIDGET(nt->nw));
	g_free(nt);
}

static gboolean do_exit(gpointer user_data)
{
	exit(0);
	return FALSE;
}

static void add_exit_timeout(NotifyDaemon* daemon)
{
	g_assert (daemon != NULL);

	if (daemon->exit_timeout_source > 0)
		return;

	daemon->exit_timeout_source = g_timeout_add_seconds(IDLE_SECONDS, do_exit, NULL);
}

static void remove_exit_timeout(NotifyDaemon* daemon)
{
	g_assert (daemon != NULL);

	if (daemon->exit_timeout_source == 0)
		return;

	g_source_remove(daemon->exit_timeout_source);
	daemon->exit_timeout_source = 0;
}

static int
_gtk_get_monitor_num (GdkMonitor *monitor)
{
	GdkDisplay *display;
	int n_monitors, i;

	display = gdk_monitor_get_display(monitor);
	n_monitors = gdk_display_get_n_monitors(display);

	for(i = 0; i < n_monitors; i++)
	{
		if (gdk_display_get_monitor(display, i) == monitor) return i;
	}

	return -1;
}

static void create_stack_for_monitor(NotifyDaemon* daemon, GdkScreen* screen, GdkMonitor *monitor_num)
{
	NotifyScreen* nscreen = daemon->screen;

	nscreen->stacks[_gtk_get_monitor_num(monitor_num)] = notify_stack_new(daemon, screen, monitor_num, daemon->stack_location);
}

static void on_screen_monitors_changed(GdkScreen* screen, NotifyDaemon* daemon)
{
	GdkDisplay     *display;
	NotifyScreen* nscreen;
	int n_monitors;
	int i;

	nscreen = daemon->screen;
	display = gdk_screen_get_display (screen);

	n_monitors = gdk_display_get_n_monitors(display);

	if (n_monitors > (int) nscreen->n_stacks)
	{
		/* grow */
		nscreen->stacks = g_renew(NotifyStack *, nscreen->stacks, (gsize) n_monitors);

		/* add more stacks */
		for (i = (int) nscreen->n_stacks; i < n_monitors; i++)
		{
			create_stack_for_monitor(daemon, screen, gdk_display_get_monitor (display, i));
		}

		nscreen->n_stacks = (gsize) n_monitors;
	}
	else if (n_monitors < (int) nscreen->n_stacks)
	{
		NotifyStack* last_stack;

		last_stack = nscreen->stacks[n_monitors - 1];

		/* transfer items before removing stacks */
		for (i = n_monitors; i < (int) nscreen->n_stacks; i++)
		{
			NotifyStack* stack = nscreen->stacks[i];
			GList* windows = g_list_copy(notify_stack_get_windows(stack));
			GList* l;

			for (l = windows; l != NULL; l = l->next)
			{
				/* skip removing the window from the old stack since it will try
				 * to unrealize the window.
				 * And the stack is going away anyhow. */
				notify_stack_add_window(last_stack, l->data, TRUE);
			}

			g_list_free(windows);
			notify_stack_destroy(stack);
			nscreen->stacks[i] = NULL;
		}

		/* remove the extra stacks */
		nscreen->stacks = g_renew(NotifyStack*, nscreen->stacks, (gsize) n_monitors);
		nscreen->n_stacks = (gsize) n_monitors;
	}
}

static void create_stacks_for_screen(NotifyDaemon* daemon, GdkScreen *screen)
{
	GdkDisplay     *display;
	NotifyScreen* nscreen;
	int i, n_monitors;

	nscreen = daemon->screen;
	display = gdk_screen_get_display (screen);
	n_monitors = gdk_display_get_n_monitors(display);

	nscreen->n_stacks = (gsize) n_monitors;

	nscreen->stacks = g_renew(NotifyStack*, nscreen->stacks, nscreen->n_stacks);

	for (i = 0; i < nscreen->n_stacks; i++)
	{
		create_stack_for_monitor(daemon, screen, gdk_display_get_monitor (display, i));
	}
}

#ifdef HAVE_X11
static GdkFilterReturn screen_xevent_filter(GdkXEvent* xevent, GdkEvent* event, NotifyScreen* nscreen)
{
	XEvent* xev = (XEvent*) xevent;

	if (xev->type == PropertyNotify && xev->xproperty.atom == nscreen->workarea_atom)
	{
		int i;

		for (i = 0; i < nscreen->n_stacks; i++)
		{
			notify_stack_queue_update_position(nscreen->stacks[i]);
		}
	}

	return GDK_FILTER_CONTINUE;
}
#endif /* HAVE_X11 */

static void create_screen(NotifyDaemon* daemon)
{
    GdkDisplay *display;
    GdkScreen  *screen;

	g_assert(daemon->screen == NULL);

	display = gdk_display_get_default();
	screen = gdk_display_get_default_screen (display);

	g_signal_connect(screen, "monitors-changed", G_CALLBACK(on_screen_monitors_changed), daemon);

	daemon->screen = g_new0(NotifyScreen, 1);

#ifdef HAVE_X11
	if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
	{
		GdkWindow  *gdkwindow;

		daemon->screen->workarea_atom = XInternAtom(GDK_DISPLAY_XDISPLAY (display), "_NET_WORKAREA", True);
		gdkwindow = gdk_screen_get_root_window(screen);
		gdk_window_add_filter(gdkwindow, (GdkFilterFunc) screen_xevent_filter, daemon->screen);
		gdk_window_set_events(gdkwindow, gdk_window_get_events(gdkwindow) | GDK_PROPERTY_CHANGE_MASK);
	}
#endif /* HAVE_X11 */

	create_stacks_for_screen(daemon, screen);
}

static void on_popup_location_changed(GSettings *settings, gchar *key, NotifyDaemon* daemon)
{
	NotifyStackLocation stack_location;
	gchar *slocation;
	int i;

	slocation = g_settings_get_string(daemon->gsettings, key);

	if (slocation != NULL && *slocation != '\0')
	{
		stack_location = get_stack_location_from_string(slocation);
	}
	else
	{
		g_settings_set_string (daemon->gsettings, GSETTINGS_KEY_POPUP_LOCATION, popup_stack_locations[POPUP_STACK_DEFAULT_INDEX].identifier);

		stack_location = NOTIFY_STACK_LOCATION_DEFAULT;
	}

	daemon->stack_location = stack_location;
	g_free(slocation);

    NotifyScreen *nscreen;

	nscreen = daemon->screen;
	for (i = 0; i < nscreen->n_stacks; i++)
	{
		NotifyStack* stack;
		stack = nscreen->stacks[i];
		notify_stack_set_location(stack, stack_location);
	}
}

static void notify_daemon_init(NotifyDaemon* daemon)
{
	gchar *location;

	daemon->next_id = 1;
	daemon->timeout_source = 0;
	daemon->skeleton = notify_daemon_notifications_skeleton_new ();

	add_exit_timeout(daemon);

	daemon->gsettings = g_settings_new (GSETTINGS_SCHEMA);

	g_signal_connect (daemon->gsettings, "changed::" GSETTINGS_KEY_POPUP_LOCATION, G_CALLBACK (on_popup_location_changed), daemon);

	location = g_settings_get_string (daemon->gsettings, GSETTINGS_KEY_POPUP_LOCATION);
	daemon->stack_location = get_stack_location_from_string(location);
	g_free(location);

	daemon->screen = NULL;

	create_screen(daemon);

	daemon->idle_reposition_notify_ids = g_hash_table_new(NULL, NULL);
	daemon->monitored_window_hash = g_hash_table_new(NULL, NULL);
	daemon->notification_hash = g_hash_table_new_full(g_int_hash, g_int_equal, g_free, (GDestroyNotify) _notify_timeout_destroy);
}

static void destroy_screen(NotifyDaemon* daemon)
{
	GdkDisplay *display;
	GdkScreen  *screen;
	gint        i;

	display = gdk_display_get_default();
	screen = gdk_display_get_default_screen (display);

	g_signal_handlers_disconnect_by_func (screen,
										  G_CALLBACK (on_screen_monitors_changed),
										  daemon);

#ifdef HAVE_X11
	if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
	{
		GdkWindow  *gdkwindow;
		gdkwindow = gdk_screen_get_root_window (screen);
		gdk_window_remove_filter (gdkwindow, (GdkFilterFunc) screen_xevent_filter, daemon->screen);
	}
#endif /* HAVE_X11 */

	for (i = 0; i < daemon->screen->n_stacks; i++) {
		 g_clear_pointer (&daemon->screen->stacks[i], notify_stack_destroy);
	}

	g_free (daemon->screen->stacks);
	daemon->screen->stacks = NULL;

	g_free(daemon->screen);
	daemon->screen = NULL;
}

static void notify_daemon_finalize(GObject* object)
{
	NotifyDaemon* daemon;

	daemon = NOTIFY_DAEMON(object);

#ifdef HAVE_X11
	if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()) &&
		g_hash_table_size(daemon->monitored_window_hash) > 0)
	{
		gdk_window_remove_filter(NULL, (GdkFilterFunc) _notify_x11_filter, daemon);
	}
#endif /* HAVE_X11 */

	if (daemon->skeleton != NULL)
	{
		GDBusInterfaceSkeleton *skeleton;

		skeleton = G_DBUS_INTERFACE_SKELETON (daemon->skeleton);
		g_dbus_interface_skeleton_unexport (skeleton);

		g_clear_object (&daemon->skeleton);
	}

	remove_exit_timeout(daemon);

	g_hash_table_destroy(daemon->monitored_window_hash);
	g_hash_table_destroy(daemon->idle_reposition_notify_ids);
	g_hash_table_destroy(daemon->notification_hash);

	destroy_screen(daemon);

	if (daemon->bus_name_id > 0)
	{
		g_bus_unown_name (daemon->bus_name_id);
		daemon->bus_name_id = 0;
	}

	G_OBJECT_CLASS(notify_daemon_parent_class)->finalize(object);
}

static NotifyStackLocation get_stack_location_from_string(const gchar *slocation)
{
	NotifyStackLocation stack_location = NOTIFY_STACK_LOCATION_DEFAULT;

	if (slocation == NULL || *slocation == '\0')
	{
		return NOTIFY_STACK_LOCATION_DEFAULT;
	}
	else
	{
		const PopupNotifyStackLocation* l;

		for (l = popup_stack_locations; l->type != NOTIFY_STACK_LOCATION_UNKNOWN; l++)
		{
			if (!strcmp(slocation, l->identifier))
			{
				stack_location = l->type;
			}
		}
	}

	return stack_location;
}

static void _action_invoked_cb(GtkWindow* nw, const char *key)
{
	NotifyDaemon* daemon;
	guint id;

	daemon = NW_GET_DAEMON(nw);
	id = NW_GET_NOTIFY_ID(nw);

	notify_daemon_notifications_emit_action_invoked (daemon->skeleton, id, key);

	_close_notification(daemon, id, TRUE, NOTIFYD_CLOSED_USER);
}

static void _emit_closed_signal(GtkWindow* nw, NotifydClosedReason reason)
{
	guint id;
	NotifyDaemon* daemon;

	id = NW_GET_NOTIFY_ID(nw);
	daemon = NW_GET_DAEMON(nw);

	notify_daemon_notifications_emit_notification_closed(daemon->skeleton, id, reason);
}

static void _close_notification(NotifyDaemon* daemon, guint id, gboolean hide_notification, NotifydClosedReason reason)
{
	NotifyTimeout* nt;

	nt = (NotifyTimeout*) g_hash_table_lookup(daemon->notification_hash, &id);

	if (nt != NULL)
	{
		_emit_closed_signal(nt->nw, reason);

		if (hide_notification)
		{
			theme_hide_notification(nt->nw);
		}

		g_hash_table_remove(daemon->notification_hash, &id);

		if (g_hash_table_size(daemon->notification_hash) == 0)
		{
			add_exit_timeout(daemon);
		}
	}
}

static gboolean _close_notification_not_shown(_NotifyPendingClose* data)
{
	_close_notification(data->daemon, data->id, TRUE, NOTIFYD_CLOSED_RESERVED);
	g_object_unref(data->daemon);
	g_free(data);

	return FALSE;
}

static void _notification_destroyed_cb(GtkWindow* nw, NotifyDaemon* daemon)
{
	/*
	 * This usually won't happen, but can if notification-daemon dies before
	 * all notifications are closed. Mark them as expired.
	 */
	_close_notification(daemon, NW_GET_NOTIFY_ID(nw), FALSE, NOTIFYD_CLOSED_EXPIRED);
}

#ifdef HAVE_X11
typedef struct {
	NotifyDaemon* daemon;
	gint id;
} IdleRepositionData;

static gboolean idle_reposition_notification(IdleRepositionData* data)
{
	NotifyDaemon* daemon;
	NotifyTimeout* nt;
	gint notify_id;

	daemon = data->daemon;
	notify_id = data->id;

	/* Look up the timeout, if it's completed we don't need to do anything */
	nt = (NotifyTimeout*) g_hash_table_lookup(daemon->notification_hash, &notify_id);

	if (nt != NULL)
	{
		sync_notification_position(daemon, nt->nw, nt->src_window_xid);
	}

	g_hash_table_remove(daemon->idle_reposition_notify_ids, GINT_TO_POINTER(notify_id));
	g_object_unref(daemon);
	g_free(data);

	return FALSE;
}

static void _queue_idle_reposition_notification(NotifyDaemon* daemon, gint notify_id)
{
	IdleRepositionData* data;
	gpointer orig_key;
	gpointer value;
	guint idle_id;

	/* Do we already have an idle update pending? */
	if (g_hash_table_lookup_extended(daemon->idle_reposition_notify_ids, GINT_TO_POINTER(notify_id), &orig_key, &value))
	{
		return;
	}

	data = g_new0(IdleRepositionData, 1);
	data->daemon = g_object_ref(daemon);
	data->id = notify_id;

	/* We do this as a short timeout to avoid repositioning spam */
	idle_id = g_timeout_add_full(G_PRIORITY_LOW, 50, (GSourceFunc) idle_reposition_notification, data, NULL);
	g_hash_table_insert(daemon->idle_reposition_notify_ids, GINT_TO_POINTER(notify_id), GUINT_TO_POINTER(idle_id));
}

static GdkFilterReturn _notify_x11_filter(GdkXEvent* xevent, GdkEvent* event, NotifyDaemon* daemon)
{
	XEvent* xev;
	gpointer orig_key;
	gpointer value;
	gint notify_id;
	NotifyTimeout* nt;

	xev = (XEvent*) xevent;

	if (xev->xany.type == DestroyNotify)
	{
		g_hash_table_remove(daemon->monitored_window_hash, GUINT_TO_POINTER(xev->xany.window));

		if (g_hash_table_size(daemon->monitored_window_hash) == 0)
		{
			gdk_window_remove_filter(NULL, (GdkFilterFunc) _notify_x11_filter, daemon);
		}

		return GDK_FILTER_CONTINUE;
	}

	if (!g_hash_table_lookup_extended(daemon->monitored_window_hash, GUINT_TO_POINTER(xev->xany.window), &orig_key, &value))
	{
		return GDK_FILTER_CONTINUE;
	}

	notify_id = GPOINTER_TO_INT(value);

	if (xev->xany.type == ConfigureNotify || xev->xany.type == MapNotify)
	{
		_queue_idle_reposition_notification(daemon, notify_id);
	}
	else if (xev->xany.type == ReparentNotify)
	{
		nt = (NotifyTimeout *) g_hash_table_lookup(daemon->notification_hash, &notify_id);

		if (nt == NULL)
		{
			return GDK_FILTER_CONTINUE;
		}

		/*
		 * If the window got reparented, we need to start monitoring the
		 * new parents.
		 */
		monitor_notification_source_windows(daemon, nt, nt->src_window_xid);
		sync_notification_position(daemon, nt->nw, nt->src_window_xid);
	}

	return GDK_FILTER_CONTINUE;
}
#endif /* HAVE_X11 */

static void _mouse_entered_cb(GtkWindow* nw, GdkEventCrossing* event, NotifyDaemon* daemon)
{
	NotifyTimeout* nt;
	guint id;

	if (event->detail == GDK_NOTIFY_INFERIOR)
	{
		return;
	}

	id = NW_GET_NOTIFY_ID(nw);
	nt = (NotifyTimeout*) g_hash_table_lookup(daemon->notification_hash, &id);

	nt->paused = TRUE;
	nt->paused_diff = nt->expiration - g_get_monotonic_time ();
}

static void _mouse_exitted_cb(GtkWindow* nw, GdkEventCrossing* event, NotifyDaemon* daemon)
{
	if (event->detail == GDK_NOTIFY_INFERIOR)
	{
		return;
	}

	guint id = NW_GET_NOTIFY_ID(nw);
	NotifyTimeout* nt = (NotifyTimeout*) g_hash_table_lookup(daemon->notification_hash, &id);

	nt->paused = FALSE;
}

static gboolean _is_expired(gpointer key, NotifyTimeout* nt, gboolean* phas_more_timeouts)
{
	gint64     now;
	GTimeSpan  time_span;

	if (!nt->has_timeout)
	{
		return FALSE;
	}

	now = g_get_monotonic_time ();
	time_span = nt->expiration - now;

	if (time_span <= 0)
	{
		theme_notification_tick(nt->nw, 0);
		_emit_closed_signal(nt->nw, NOTIFYD_CLOSED_EXPIRED);
		return TRUE;
	}
	else if (nt->paused)
	{
		nt->expiration = now + nt->paused_diff;
	}
	else
	{
		theme_notification_tick (nt->nw, time_span / G_TIME_SPAN_MILLISECOND);
	}

	*phas_more_timeouts = TRUE;

	return FALSE;
}

static gboolean _check_expiration(NotifyDaemon* daemon)
{
	gboolean has_more_timeouts = FALSE;

	g_hash_table_foreach_remove(daemon->notification_hash, (GHRFunc) _is_expired, (gpointer) &has_more_timeouts);

	if (!has_more_timeouts)
	{
		daemon->timeout_source = 0;

		if (g_hash_table_size (daemon->notification_hash) == 0)
		{
			add_exit_timeout(daemon);
		}
	}

	return has_more_timeouts;
}

static void _calculate_timeout(NotifyDaemon* daemon, NotifyTimeout* nt, int timeout)
{
	if (timeout == 0)
	{
		nt->has_timeout = FALSE;
	}
	else
	{
		gint64 usec;

		nt->has_timeout = TRUE;

		if (timeout == -1)
		{
			timeout = NOTIFY_DAEMON_DEFAULT_TIMEOUT;
		}

		theme_set_notification_timeout(nt->nw, timeout);

		usec = (gint64) timeout * G_TIME_SPAN_MILLISECOND;  /* convert from msec to usec */

		nt->expiration = usec + g_get_monotonic_time ();

		if (daemon->timeout_source == 0)
		{
			daemon->timeout_source = g_timeout_add(100, (GSourceFunc) _check_expiration, daemon);
		}
	}
}

static guint _generate_id(NotifyDaemon* daemon)
{
	guint id = 0;

	do {
		id = daemon->next_id;

		if (id != UINT_MAX)
		{
			daemon->next_id++;
		}
		else
		{
			daemon->next_id = 1;
		}

		if (g_hash_table_lookup (daemon->notification_hash, &id) != NULL)
		{
			id = 0;
		}

	} while (id == 0);

	return id;
}

static NotifyTimeout* _store_notification(NotifyDaemon* daemon, GtkWindow* nw, int timeout)
{
	NotifyTimeout* nt;
	guint id = _generate_id(daemon);

	nt = g_new0(NotifyTimeout, 1);
	nt->id = id;
	nt->nw = nw;
	nt->daemon = daemon;

	_calculate_timeout(daemon, nt, timeout);

#if GLIB_CHECK_VERSION (2, 68, 0)
	g_hash_table_insert(daemon->notification_hash, g_memdup2(&id, sizeof(guint)), nt);
#else
	g_hash_table_insert(daemon->notification_hash, g_memdup(&id, sizeof(guint)), nt);
#endif
	remove_exit_timeout(daemon);

	return nt;
}

static GdkPixbuf * _notify_daemon_pixbuf_from_data_hint (GVariant *icon_data)
{
        gboolean        has_alpha;
        int             bits_per_sample;
        int             width;
        int             height;
        int             rowstride;
        int             n_channels;
        GVariant       *data_variant;
        gsize           expected_len;
        guchar         *data;
        gsize           data_size;
        GdkPixbuf      *pixbuf;

        g_variant_get (icon_data,
                       "(iiibii@ay)",
                       &width,
                       &height,
                       &rowstride,
                       &has_alpha,
                       &bits_per_sample,
                       &n_channels,
                       &data_variant);

        expected_len = (gsize) ((height - 1) * rowstride + width * ((n_channels * bits_per_sample + 7) / 8));

        if (expected_len != g_variant_get_size (data_variant)) {
                g_warning ("Expected image data to be of length %" G_GSIZE_FORMAT
                           " but got a " "length of %" G_GSIZE_FORMAT,
                           expected_len,
                           g_variant_get_size (data_variant));
                return NULL;
        }

        data_size = g_variant_get_size (data_variant);
#if GLIB_CHECK_VERSION (2, 68, 0)
        data = (guchar *) g_memdup2 (g_variant_get_data (data_variant), data_size);
#else
        data = (guchar *) g_memdup (g_variant_get_data (data_variant), (guint) data_size);
#endif
        pixbuf = gdk_pixbuf_new_from_data (data,
                                           GDK_COLORSPACE_RGB,
                                           has_alpha,
                                           bits_per_sample,
                                           width,
                                           height,
                                           rowstride,
                                           (GdkPixbufDestroyNotify) g_free,
                                           NULL);

        return pixbuf;
}

static GdkPixbuf* _notify_daemon_pixbuf_from_path(const char* path)
{
	GdkPixbuf* pixbuf = NULL;

	if (!strncmp (path, "file://", 7) || *path == '/')
	{
		if (!strncmp (path, "file://", 7))
		{
			path += 7;

			/* Unescape URI-encoded, allowed characters */
			path = g_uri_unescape_string (path, G_URI_RESERVED_CHARS_ALLOWED_IN_PATH);
		}

		/* Load file */
		pixbuf = gdk_pixbuf_new_from_file (path, NULL);
	}
	else
	{
		/* Load icon theme icon */
		GtkIconTheme *theme;
		GtkIconInfo  *icon_info;

		theme = gtk_icon_theme_get_default ();
		icon_info = gtk_icon_theme_lookup_icon (theme, path, IMAGE_SIZE, GTK_ICON_LOOKUP_USE_BUILTIN);

		if (icon_info != NULL)
		{
			gint icon_size;

			icon_size = MIN (IMAGE_SIZE, gtk_icon_info_get_base_size (icon_info));

			if (icon_size == 0)
			{
				icon_size = IMAGE_SIZE;
			}

			pixbuf = gtk_icon_theme_load_icon (theme, path, icon_size, GTK_ICON_LOOKUP_USE_BUILTIN, NULL);

			g_object_unref (icon_info);
		}

		if (pixbuf == NULL)
		{
			/* Well... maybe this is a file afterall. */
			pixbuf = gdk_pixbuf_new_from_file (path, NULL);
		}
	}

	return pixbuf;
}

static GdkPixbuf* _notify_daemon_scale_pixbuf(GdkPixbuf *pixbuf, gboolean no_stretch_hint)
{
	int pw;
	int ph;
	float scale_factor;

	pw = gdk_pixbuf_get_width (pixbuf);
	ph = gdk_pixbuf_get_height (pixbuf);

	/* Determine which dimension requires the smallest scale. */
	scale_factor = (float) IMAGE_SIZE / (float) MAX(pw, ph);

	/* always scale down, allow to disable scaling up */
	if (scale_factor < 1.0 || !no_stretch_hint)
	{
		int scale_x;
		int scale_y;

		scale_x = (int) (((float) pw) * scale_factor);
		scale_y = (int) (((float) ph) * scale_factor);
		return gdk_pixbuf_scale_simple (pixbuf,
		                                scale_x,
		                                scale_y,
		                                GDK_INTERP_BILINEAR);
	}
	else
	{
		return g_object_ref (pixbuf);
	}
}

static void window_clicked_cb(GtkWindow* nw, GdkEventButton* button, NotifyDaemon* daemon)
{
	if (daemon->url_clicked_lock)
	{
		daemon->url_clicked_lock = FALSE;
		return;
	}

	_action_invoked_cb (nw, "default");
	_close_notification (daemon, NW_GET_NOTIFY_ID (nw), TRUE, NOTIFYD_CLOSED_USER);
}

static void url_clicked_cb(GtkWindow* nw, const char *url)
{
	NotifyDaemon* daemon;
	gchar *escaped_url;
	gchar *cmd = NULL;
	gchar *found = NULL;

	daemon = NW_GET_DAEMON(nw);

	/* Somewhat of a hack.. */
	daemon->url_clicked_lock = TRUE;

	escaped_url = g_shell_quote (url);

	if ((found = g_find_program_in_path ("gio")) != NULL)
	{
		cmd = g_strdup_printf ("gio open %s", escaped_url);
	}
	else if ((found = g_find_program_in_path ("xdg-open")) != NULL)
	{
		cmd = g_strdup_printf ("xdg-open %s", escaped_url);
	}
	else if ((found = g_find_program_in_path ("firefox")) != NULL)
	{
		cmd = g_strdup_printf ("firefox %s", escaped_url);
	}
	else
	{
		g_warning ("Unable to find a browser.");
	}

	g_free (found);
	g_free (escaped_url);

	if (cmd != NULL)
	{
		g_spawn_command_line_async (cmd, NULL);
		g_free (cmd);
	}
}

static gboolean screensaver_active(GtkWidget* nw)
{
	GError* error = NULL;
	gboolean active = FALSE;
	GVariant *variant;
	GDBusProxy *proxy = NULL;

	proxy = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION,
			G_DBUS_PROXY_FLAGS_NONE,
			NULL,
			"org.mate.ScreenSaver",
			"/",
			"org.mate.ScreenSaver",
			NULL,
			&error);
	if (proxy == NULL) {
		g_warning("Failed to get dbus connection: %s", error->message);
		g_error_free (error);
	}

	variant = g_dbus_proxy_call_sync (proxy,
			"GetActive",
			g_variant_new ("()"),
			G_DBUS_CALL_FLAGS_NONE,
			-1,
			NULL,
			&error);
	if (variant == NULL)
	{
		//g_warning("Failed to call mate-screensaver: %s", error->message);
		g_error_free (error);
		return active;
	}

	g_variant_get (variant, "(b)", &active);
	g_variant_unref (variant);
	g_object_unref (proxy);
	return active;
}

#ifdef HAVE_X11
static gboolean fullscreen_window_exists(GtkWidget* nw)
{
	WnckScreen* wnck_screen;
	WnckWorkspace* wnck_workspace;
	GList* l;

	g_return_val_if_fail (GDK_IS_X11_DISPLAY (gdk_display_get_default ()), FALSE);

	wnck_screen = wnck_screen_get (GDK_SCREEN_XNUMBER (gdk_window_get_screen (gtk_widget_get_window (nw))));

	wnck_screen_force_update (wnck_screen);

	wnck_workspace = wnck_screen_get_active_workspace (wnck_screen);

	if (!wnck_workspace)
	{
		return FALSE;
	}

	for (l = wnck_screen_get_windows_stacked (wnck_screen); l != NULL; l = l->next)
	{
		WnckWindow *wnck_win = (WnckWindow *) l->data;

		if (wnck_window_is_on_workspace (wnck_win, wnck_workspace) && wnck_window_is_fullscreen (wnck_win) && wnck_window_is_active (wnck_win))
		{
			/*
			 * Sanity check if the window is _really_ fullscreen to
			 * work around a bug in libwnck that doesn't get all
			 * unfullscreen events.
			 */
			int sw = wnck_screen_get_width (wnck_screen);
			int sh = wnck_screen_get_height (wnck_screen);
			int x, y, w, h;

			wnck_window_get_geometry (wnck_win, &x, &y, &w, &h);

			if (sw == w && sh == h)
			{
				return TRUE;
			}
		}
	}

	return FALSE;
}

static Window get_window_parent(GdkDisplay* display, Window window, Window* root)
{
	Window parent;
	Window* children = NULL;
	guint nchildren;
	gboolean result;

	gdk_x11_display_error_trap_push (display);
	result = XQueryTree(GDK_DISPLAY_XDISPLAY(display), window, root, &parent, &children, &nchildren);

	if (gdk_x11_display_error_trap_pop (display) || !result)
	{
		return None;
	}

	if (children)
	{
		XFree(children);
	}

	return parent;
}

/*
 * Recurse over X Window and parents, up to root, and start watching them
 * for position changes.
 */
static void monitor_notification_source_windows(NotifyDaemon  *daemon, NotifyTimeout *nt, Window source)
{
	GdkDisplay *display;
	Window root = None;
	Window parent;

	display = gdk_display_get_default ();

	/* Start monitoring events if necessary.  We don't want to
	   filter events unless we absolutely have to. */
	if (g_hash_table_size (daemon->monitored_window_hash) == 0)
	{
		gdk_window_add_filter (NULL, (GdkFilterFunc) _notify_x11_filter, daemon);
	}

	/* Store the window in the timeout */
	g_assert (nt != NULL);
	nt->src_window_xid = source;

	for (parent = get_window_parent (display, source, &root); parent != None && root != parent; parent = get_window_parent (display, parent, &root))
	{
		XSelectInput (GDK_DISPLAY_XDISPLAY(display), parent, StructureNotifyMask);

		g_hash_table_insert(daemon->monitored_window_hash, GUINT_TO_POINTER (parent), GINT_TO_POINTER (nt->id));
	}
}

/* Use a source X Window ID to reposition a notification. */
static void sync_notification_position(NotifyDaemon* daemon, GtkWindow* nw, Window source)
{
	GdkDisplay *display;
	Status result;
	Window root;
	Window child;
	int x, y;
	unsigned int width, height;
	unsigned int border_width, depth;

	display = gdk_display_get_default ();

	gdk_x11_display_error_trap_push (display);

	/* Get the root for this window */
	result = XGetGeometry(GDK_DISPLAY_XDISPLAY(display), source, &root, &x, &y, &width, &height, &border_width, &depth);

	if (gdk_x11_display_error_trap_pop (display) || !result)
	{
		return;
	}

	/*
	 * Now calculate the offset coordinates for the source window from
	 * the root.
	 */
	gdk_x11_display_error_trap_push (display);
	result = XTranslateCoordinates (GDK_DISPLAY_XDISPLAY (display), source, root, 0, 0, &x, &y, &child);
	if (gdk_x11_display_error_trap_pop (display) || !result)
	{
		return;
	}

	x += (int)width / 2;
	y += (int)height / 2;

	theme_set_notification_arrow (nw, TRUE, x, y);
	theme_move_notification (nw, x, y);
	theme_show_notification (nw);

	/*
	 * We need to manually queue a draw here as the default theme recalculates
	 * its position in the draw handler and moves the window (which seems
	 * fairly broken), so just calling move/show above isn't enough to cause
	 * its position to be calculated.
	 */
	gtk_widget_queue_draw (GTK_WIDGET (nw));
}
#endif /* HAVE_X11 */

GQuark notify_daemon_error_quark(void)
{
	static GQuark q;

	if (q == 0)
	{
		q = g_quark_from_static_string ("notification-daemon-error-quark");
	}

	return q;
}

static gboolean notify_daemon_notify_handler(NotifyDaemonNotifications *object, GDBusMethodInvocation *invocation, const gchar *app_name, guint id, const gchar *icon, const gchar *summary, const gchar *body, const gchar *const *actions, GVariant *hints, gint timeout, gpointer user_data)
{
	NotifyDaemon *daemon;
	daemon = NOTIFY_DAEMON (user_data);
	NotifyTimeout* nt = NULL;
	GtkWindow* nw = NULL;
	GVariant* data;
	gboolean use_pos_data = FALSE;
	gboolean new_notification = FALSE;
	gint x = 0;
	gint y = 0;
	guint return_id;
	char* sound_file = NULL;
	gboolean sound_enabled;
	gboolean do_not_disturb;
	gint i;
	GdkPixbuf* pixbuf;
	GSettings* gsettings;
	gboolean fullscreen_window;

#ifdef HAVE_X11
	Window window_xid = None;
#endif /* HAVE_X11 */

	if (g_hash_table_size (daemon->notification_hash) > MAX_NOTIFICATIONS)
	{
		g_dbus_method_invocation_return_error (invocation, notify_daemon_error_quark(), 1, _("Exceeded maximum number of notifications"));
		return FALSE;
	}

	/* Grab the settings */
	gsettings = g_settings_new (GSETTINGS_SCHEMA);
	sound_enabled = g_settings_get_boolean (gsettings, GSETTINGS_KEY_SOUND_ENABLED);
	do_not_disturb = g_settings_get_boolean (gsettings, GSETTINGS_KEY_DO_NOT_DISTURB);
	g_object_unref (gsettings);

	/* If we are in do-not-disturb mode, just grab a new id and close the notification */
	if (do_not_disturb)
	{
		return_id = _generate_id (daemon);
		notify_daemon_notifications_complete_notify (object, invocation, return_id);
		return TRUE;
	}

	if (id > 0)
	{
		nt = (NotifyTimeout *) g_hash_table_lookup (daemon->notification_hash, &id);

		if (nt != NULL)
		{
			nw = nt->nw;
		}
		else
		{
			id = 0;
		}
	}

	if (nw == NULL)
	{
		nw = theme_create_notification (url_clicked_cb);
		g_object_set_data (G_OBJECT (nw), "_notify_daemon", daemon);
		gtk_widget_realize (GTK_WIDGET (nw));
		new_notification = TRUE;

		g_signal_connect (G_OBJECT (nw), "button-release-event", G_CALLBACK (window_clicked_cb), daemon);
		g_signal_connect (G_OBJECT (nw), "destroy", G_CALLBACK (_notification_destroyed_cb), daemon);
		g_signal_connect (G_OBJECT (nw), "enter-notify-event", G_CALLBACK (_mouse_entered_cb), daemon);
		g_signal_connect (G_OBJECT (nw), "leave-notify-event", G_CALLBACK (_mouse_exitted_cb), daemon);
	}
	else
	{
		theme_clear_notification_actions (nw);
	}

	theme_set_notification_text (nw, summary, body);
	theme_set_notification_hints (nw, hints);

	/*
	 *XXX This needs to handle file URIs and all that.
	 */

#ifdef HAVE_X11
	if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()) &&
		g_variant_lookup(hints, "window-xid", "@u", &data))
	{
		window_xid = (Window) g_variant_get_uint32 (data);
		g_variant_unref(data);
	} else
#endif /* HAVE_X11 */
	/* deal with x, and y hints */
	if (g_variant_lookup(hints, "x", "i", &x))
	{
		if (g_variant_lookup(hints, "y", "i", &y))
		{
			use_pos_data = TRUE;
		}
	}

	if (g_variant_lookup(hints, "suppress-sound", "@*", &data))
	{
		if (g_variant_is_of_type (data, G_VARIANT_TYPE_BOOLEAN))
		{
			sound_enabled = !g_variant_get_boolean(data);
		}
		else if (g_variant_is_of_type (data, G_VARIANT_TYPE_INT32))
		{
			sound_enabled = (g_variant_get_int32(data) == 0);
		}
		else
		{
			g_warning ("suppress-sound is of type %s (expected bool or int)\n", g_variant_get_type_string(data));
		}
		g_variant_unref(data);
	}

	if (sound_enabled)
	{
		if (g_variant_lookup(hints, "sound-file", "s", &sound_file))
		{
			if (*sound_file == '\0' || !g_file_test (sound_file, G_FILE_TEST_EXISTS))
			{
				g_free (sound_file);
				sound_file = NULL;
			}
		}
	}

	/* set up action buttons */
	for (i = 0; actions[i] != NULL; i += 2)
	{
		if (actions[i+1] == NULL)
		{
			g_warning ("Label not found for action %s. The protocol specifies that a label must follow an action in the actions array", actions[i]);

			break;
		}

		if (strcasecmp (actions[i], "default"))
		{
			theme_add_notification_action (nw, actions[i+1], actions[i], G_CALLBACK (_action_invoked_cb));
		}
	}

	pixbuf = NULL;

	if (g_variant_lookup(hints, "image_data", "@(iiibiiay)", &data))
	{
		pixbuf = _notify_daemon_pixbuf_from_data_hint (data);
		g_variant_unref(data);
	}
	else if (g_variant_lookup(hints, "image-data", "@(iiibiiay)", &data))
	{
		pixbuf = _notify_daemon_pixbuf_from_data_hint (data);
		g_variant_unref(data);
	}
	else if (g_variant_lookup(hints, "image_path", "@s", &data))
	{
		const char *path = g_variant_get_string (data, NULL);
		pixbuf = _notify_daemon_pixbuf_from_path (path);
		g_variant_unref(data);
	}
	else if (g_variant_lookup(hints, "image-path", "@s", &data))
	{
		const char *path = g_variant_get_string (data, NULL);
		pixbuf = _notify_daemon_pixbuf_from_path (path);
		g_variant_unref(data);
	}
	else if (*icon != '\0')
	{
		pixbuf = _notify_daemon_pixbuf_from_path (icon);
	}
	else if (g_variant_lookup(hints, "icon_data", "@(iiibiiay)", &data))
	{
		g_warning("\"icon_data\" hint is deprecated, please use \"image_data\" instead");
		pixbuf = _notify_daemon_pixbuf_from_data_hint (data);
		g_variant_unref(data);
	}

	if (pixbuf != NULL)
	{
		GdkPixbuf *scaled;
		scaled = NULL;
		scaled = _notify_daemon_scale_pixbuf (pixbuf, TRUE);
		theme_set_notification_icon (nw, scaled);
		g_object_unref (G_OBJECT (pixbuf));
		if (scaled != NULL)
			g_object_unref (scaled);
	}

#ifdef HAVE_X11
	if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()) &&
	    window_xid != None &&
	    !theme_get_always_stack (nw))
	{
		/*
		 * Do nothing here if we were passed an XID; we'll call
		 * sync_notification_position later.
		 */
	}
	else
#endif /* HAVE_X11 */
	if (use_pos_data && !theme_get_always_stack (nw))
	{
		/*
		 * Typically, the theme engine will set its own position based on
		 * the arrow X, Y hints. However, in case, move the notification to
		 * that position.
		 */
		theme_set_notification_arrow (nw, TRUE, x, y);
		theme_move_notification (nw, x, y);
	}
	else
	{
		GdkMonitor *monitor_id;
		GdkDisplay *display;
		GdkSeat *seat;
		GdkDevice *pointer;
		GdkScreen* screen;

		theme_set_notification_arrow (nw, FALSE, 0, 0);

		/* If the "use-active-monitor" gsettings key is set to TRUE, then
		 * get the monitor the pointer is at. Otherwise, get the monitor
		 * number the user has set in gsettings. */
		if (g_settings_get_boolean(daemon->gsettings, GSETTINGS_KEY_USE_ACTIVE_MONITOR))
		{
			gint coordinate_x, coordinate_y;

			display = gdk_display_get_default ();
			seat = gdk_display_get_default_seat (display);
			pointer = gdk_seat_get_pointer (seat);

			gdk_device_get_position (pointer,
			                         &screen,
			                         &coordinate_x,
			                         &coordinate_y);
			monitor_id = gdk_display_get_monitor_at_point (gdk_screen_get_display (screen),
			                                               coordinate_x,
			                                               coordinate_y);
		}
		else
		{
			screen = gdk_display_get_default_screen(gdk_display_get_default());
			monitor_id = gdk_display_get_monitor (gdk_display_get_default(),
							      g_settings_get_int(daemon->gsettings, GSETTINGS_KEY_MONITOR_NUMBER));
		}

		if (_gtk_get_monitor_num (monitor_id) >= daemon->screen->n_stacks)
		{
			/* screw it - dump it on the last one we'll get
			 a monitors-changed signal soon enough*/
			monitor_id = gdk_display_get_monitor (gdk_display_get_default(), (int) daemon->screen->n_stacks - 1);
		}

		notify_stack_add_window (daemon->screen->stacks[_gtk_get_monitor_num (monitor_id)], nw, new_notification);
	}

	if (id == 0)
	{
		nt = _store_notification (daemon, nw, timeout);
		return_id = nt->id;
	}
	else
	{
		return_id = id;
	}

#ifdef HAVE_X11
	/*
	 * If we have a source Window XID, start monitoring the tree
	 * for changes, and reposition the window based on the source
	 * window.  We need to do this after return_id is calculated.
	 */
	if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()) &&
		window_xid != None &&
		!theme_get_always_stack (nw))
	{
		monitor_notification_source_windows (daemon, nt, window_xid);
		sync_notification_position (daemon, nw, window_xid);
	}
#endif /* HAVE_X11 */

	fullscreen_window = FALSE;
#ifdef HAVE_X11
	if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
		fullscreen_window = fullscreen_window_exists (GTK_WIDGET (nw));
#endif /* HAVE_X11 */
	/* fullscreen_window is assumed to be false on Wayland, as there is no trivial way to check */

	/* If there is no timeout, show the notification also if screensaver
	 * is active or there are fullscreen windows
	 */
	if (!nt->has_timeout || (!screensaver_active (GTK_WIDGET (nw)) && !fullscreen_window))
	{
		theme_show_notification (nw);

		if (sound_file != NULL)
		{
			sound_play_file (GTK_WIDGET (nw), sound_file);
		}
	}
	else
	{
		_NotifyPendingClose *notification_data;

		/* The notification was not shown, so queue up a close
		 * for it */
		notification_data = g_new0 (_NotifyPendingClose, 1);
		notification_data->id = id;
		notification_data->daemon = g_object_ref (daemon);
		g_idle_add ((GSourceFunc) _close_notification_not_shown, notification_data);
	}

	g_free (sound_file);

	g_object_set_data (G_OBJECT (nw), "_notify_id", GUINT_TO_POINTER (return_id));

	if (nt)
	{
		_calculate_timeout (daemon, nt, timeout);
	}

	notify_daemon_notifications_complete_notify ( object, invocation, return_id);
	return TRUE;
}

static gboolean notify_daemon_close_notification_handler(NotifyDaemonNotifications *object, GDBusMethodInvocation *invocation, guint id, gpointer user_data)
{
	if (id == 0)
	{
		g_dbus_method_invocation_return_error (invocation, notify_daemon_error_quark(), 100,  _("%u is not a valid notification ID"), id);
		return FALSE;
	}
	else
	{
		NotifyDaemon *daemon;
		daemon = NOTIFY_DAEMON (user_data);
		_close_notification (daemon, id, TRUE, NOTIFYD_CLOSED_API);
		notify_daemon_notifications_complete_close_notification (object, invocation);
		return TRUE;
	}
}

static gboolean notify_daemon_get_capabilities( NotifyDaemonNotifications *object, GDBusMethodInvocation *invocation)
{
	GVariantBuilder *builder;
	GVariant *value;

	builder = g_variant_builder_new (G_VARIANT_TYPE ("as"));
	g_variant_builder_add (builder, "s", "actions");
	g_variant_builder_add (builder, "s", "action-icons");
	g_variant_builder_add (builder, "s", "body");
	g_variant_builder_add (builder, "s", "body-hyperlinks");
	g_variant_builder_add (builder, "s", "body-markup");
	g_variant_builder_add (builder, "s", "icon-static");
	g_variant_builder_add (builder, "s", "sound");
	value = g_variant_new ("as", builder);
	g_variant_builder_unref (builder);
	notify_daemon_notifications_complete_get_capabilities (
			object,
			invocation,
			(const gchar* const *)g_variant_dup_strv (value, NULL));
	g_variant_unref (value);
	return TRUE;
}

static gboolean notify_daemon_get_server_information (NotifyDaemonNotifications *object, GDBusMethodInvocation *invocation, gpointer user_data)
{
	notify_daemon_notifications_complete_get_server_information(object,
			invocation,
			"Notification Daemon",
			"MATE",
			PACKAGE_VERSION,
			"1.1");
	return TRUE;
}

NotifyDaemon* notify_daemon_new (gboolean replace)
{
	return g_object_new (NOTIFY_TYPE_DAEMON, "replace", replace, NULL);
}
