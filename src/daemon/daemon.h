/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2006 Christian Hammond <chipx86@chipx86.com>
 * Copyright (C) 2005 John (J5) Palmieri <johnp@redhat.com>
 * Copyright (C) 2010 Red Hat, Inc.
 * Copyright (C) 2011 Perberos <perberos@gmail.com>
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

#ifndef NOTIFY_DAEMON_H
#define NOTIFY_DAEMON_H

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#define GSETTINGS_SCHEMA             "org.mate.NotificationDaemon"
#define GSETTINGS_KEY_THEME          "theme"
#define GSETTINGS_KEY_POPUP_LOCATION "popup-location"
#define GSETTINGS_KEY_SOUND_ENABLED  "sound-enabled"
#define GSETTINGS_KEY_DO_NOT_DISTURB "do-not-disturb"
#define GSETTINGS_KEY_MONITOR_NUMBER "monitor-number"
#define GSETTINGS_KEY_USE_ACTIVE     "use-active-monitor"

#define NOTIFY_TYPE_DAEMON (notify_daemon_get_type())
G_DECLARE_FINAL_TYPE (NotifyDaemon, notify_daemon, NOTIFY, DAEMON, GObject)

#define NOTIFY_DAEMON_DEFAULT_TIMEOUT 7000
extern int exit_timeout_seconds;

enum {
	URGENCY_LOW,
	URGENCY_NORMAL,
	URGENCY_CRITICAL
};

typedef enum {
	NOTIFYD_CLOSED_EXPIRED = 1,
	NOTIFYD_CLOSED_USER = 2,
	NOTIFYD_CLOSED_API = 3,
	NOTIFYD_CLOSED_RESERVED = 4
} NotifydClosedReason;

G_BEGIN_DECLS

NotifyDaemon*     notify_daemon_new        (gboolean replace);

GQuark notify_daemon_error_quark(void);

G_END_DECLS
#endif /* NOTIFY_DAEMON_H */
