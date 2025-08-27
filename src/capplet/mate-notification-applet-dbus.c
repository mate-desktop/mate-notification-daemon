/* mate-notification-applet-dbus.c - MATE Notification Applet - D-Bus Context
 *
 * Copyright (C) 2025 MATE Developers
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include "config.h"

#include <glib.h>
#include <gio/gio.h>

#include "mate-notification-applet-dbus.h"
#include "../common/constants.h"

MateNotificationDBusContext *
dbus_context_new (void)
{
  MateNotificationDBusContext *context = g_new0 (MateNotificationDBusContext, 1);
  context->daemon_proxy = NULL;
  context->daemon_available = FALSE;
  context->watch_id = 0;
  return context;
}

void
dbus_context_free (MateNotificationDBusContext *context)
{
  if (context) {
    dbus_context_disconnect (context);
    g_free (context);
  }
}

gboolean
dbus_context_connect (MateNotificationDBusContext *context)
{
  GError *error = NULL;

  if (!context)
    return FALSE;

  /* Clean up existing connection */
  dbus_context_disconnect (context);

  /* Create D-Bus proxy for notification daemon */
  context->daemon_proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                         G_DBUS_PROXY_FLAGS_NONE,
                                                         NULL, /* GDBusInterfaceInfo */
                                                         NOTIFICATION_BUS_NAME,
                                                         NOTIFICATION_BUS_PATH,
                                                         "org.freedesktop.Notifications",
                                                         NULL, /* GCancellable */
                                                         &error);

  if (error) {
    g_warning ("Failed to connect to notification daemon: %s", error->message);
    g_error_free (error);
    context->daemon_available = FALSE;
    return FALSE;
  }

  context->daemon_available = TRUE;
  return TRUE;
}

void
dbus_context_disconnect (MateNotificationDBusContext *context)
{
  if (!context)
    return;

  if (context->daemon_proxy) {
    g_object_unref (context->daemon_proxy);
    context->daemon_proxy = NULL;
  }

  context->daemon_available = FALSE;
}

gboolean
dbus_context_is_available (MateNotificationDBusContext *context)
{
  return context && context->daemon_available && context->daemon_proxy;
}

guint
dbus_context_get_notification_count (MateNotificationDBusContext *context)
{
  GVariant *result;
  GError *error = NULL;
  guint count = 0;

  if (!dbus_context_is_available (context))
    return 0;

  result = g_dbus_proxy_call_sync (context->daemon_proxy,
                                   "GetNotificationCount",
                                   NULL,
                                   G_DBUS_CALL_FLAGS_NONE,
                                   -1, /* timeout */
                                   NULL, /* GCancellable */
                                   &error);

  if (error) {
    g_warning ("Failed to get notification count: %s", error->message);
    g_error_free (error);
    context->daemon_available = FALSE;
  } else if (result) {
    g_variant_get (result, "(u)", &count);
    g_variant_unref (result);
  }

  return count;
}

gboolean
dbus_context_clear_notification_history (MateNotificationDBusContext *context)
{
  GVariant *result;
  GError *error = NULL;

  if (!dbus_context_is_available (context))
    return FALSE;

  result = g_dbus_proxy_call_sync (context->daemon_proxy,
                                   "ClearNotificationHistory",
                                   NULL,
                                   G_DBUS_CALL_FLAGS_NONE,
                                   -1, /* timeout */
                                   NULL, /* GCancellable */
                                   &error);

  if (error) {
    g_warning ("Failed to clear notification history: %s", error->message);
    g_error_free (error);
    return FALSE;
  }

  if (result) {
    g_variant_unref (result);
  }

  return TRUE;
}

gboolean
dbus_context_mark_all_notifications_as_read (MateNotificationDBusContext *context)
{
  GVariant *result;
  GError *error = NULL;

  if (!dbus_context_is_available (context))
    return FALSE;

  result = g_dbus_proxy_call_sync (context->daemon_proxy,
                                   "MarkAllNotificationsAsRead",
                                   NULL,
                                   G_DBUS_CALL_FLAGS_NONE,
                                   -1, /* timeout */
                                   NULL, /* GCancellable */
                                   &error);

  if (error) {
    g_warning ("Failed to mark all notifications as read: %s", error->message);
    g_error_free (error);
    return FALSE;
  }

  if (result) {
    g_variant_unref (result);
  }

  return TRUE;
}

GVariant *
dbus_context_get_notification_history (MateNotificationDBusContext *context)
{
  GVariant *result;
  GError *error = NULL;

  if (!dbus_context_is_available (context))
    return NULL;

  result = g_dbus_proxy_call_sync (context->daemon_proxy,
                                   "GetNotificationHistory",
                                   NULL,
                                   G_DBUS_CALL_FLAGS_NONE,
                                   -1, /* timeout */
                                   NULL, /* GCancellable */
                                   &error);

  if (error) {
    g_warning ("Failed to get notification history: %s", error->message);
    g_error_free (error);
    return NULL;
  }

  return result; /* Caller owns this reference */
}
