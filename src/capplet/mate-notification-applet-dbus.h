/* mate-notification-applet-dbus.h - MATE Notification Applet - D-Bus Context
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

#ifndef __MATE_NOTIFICATION_APPLET_DBUS_H__
#define __MATE_NOTIFICATION_APPLET_DBUS_H__

#include <gio/gio.h>

G_BEGIN_DECLS

/* D-Bus context structure for notification daemon communication */
typedef struct {
  GDBusProxy *daemon_proxy;
  gboolean    daemon_available;
  guint       watch_id;
} MateNotificationDBusContext;

/* Context creation/cleanup */
MateNotificationDBusContext *dbus_context_new (void);
void dbus_context_free (MateNotificationDBusContext *context);

/* Connection management */
gboolean dbus_context_connect (MateNotificationDBusContext *context);
void dbus_context_disconnect (MateNotificationDBusContext *context);
gboolean dbus_context_is_available (MateNotificationDBusContext *context);

/* Notification daemon method calls */
guint dbus_context_get_notification_count (MateNotificationDBusContext *context);
gboolean dbus_context_clear_notification_history (MateNotificationDBusContext *context);
gboolean dbus_context_mark_all_notifications_as_read (MateNotificationDBusContext *context);
GVariant *dbus_context_get_notification_history (MateNotificationDBusContext *context);

G_END_DECLS

#endif /* __MATE_NOTIFICATION_APPLET_DBUS_H__ */
