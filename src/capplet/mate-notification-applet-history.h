/* mate-notification-applet.c - MATE Notification Applet - History
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

#ifndef __MATE_NOTIFICATION_APPLET_HISTORY_H__
#define __MATE_NOTIFICATION_APPLET_HISTORY_H__

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <mate-panel-applet.h>

#include "mate-notification-applet-dbus.h"

G_BEGIN_DECLS

/* History context structure */
typedef struct {
  MateNotificationDBusContext *dbus_context;
  GtkWidget                   *main_widget;
  GtkWidget                   *history_popup;
  GCallback                    count_update_callback;
  gpointer                     count_update_user_data;
  GSettings                   *settings;
} MateNotificationHistoryContext;

/* History popup functions */
void show_notification_history (MateNotificationHistoryContext *context);

/* Context creation/cleanup */
MateNotificationHistoryContext *history_context_new (MateNotificationDBusContext *dbus_context,
                                                     GtkWidget                   *main_widget,
                                                     GCallback                    count_update_callback,
                                                     gpointer                     count_update_user_data,
                                                     GSettings                   *settings);
void history_context_free                           (MateNotificationHistoryContext *context);

/* Context update functions */
void history_context_update_dbus (MateNotificationHistoryContext *context,
                                  MateNotificationDBusContext    *dbus_context);

G_END_DECLS

#endif /* __MATE_NOTIFICATION_APPLET_HISTORY_H__ */
