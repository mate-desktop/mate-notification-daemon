/*
 * Copyright (C) 2020 Robert Buj <robert.buj@gmail.com>
 * Copyright (C) 2020-2021 MATE Developers
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

#ifndef _CONSTANTS_H_
#define _CONSTANTS_H_

#define GSETTINGS_SCHEMA                 "org.mate.NotificationDaemon"
#define GSETTINGS_KEY_THEME              "theme"
#define GSETTINGS_KEY_DO_NOT_DISTURB     "do-not-disturb"
#define GSETTINGS_KEY_MONITOR_NUMBER     "monitor-number"
#define GSETTINGS_KEY_POPUP_LOCATION     "popup-location"
#define GSETTINGS_KEY_SOUND_ENABLED      "sound-enabled"
#define GSETTINGS_KEY_USE_ACTIVE_MONITOR "use-active-monitor"
#define GSETTINGS_KEY_DEFAULT_TIMEOUT    "default-timeout"
#define GSETTINGS_KEY_ENABLE_PERSISTENCE "enable-persistence"
#define GSETTINGS_KEY_SHOW_COUNTDOWN     "show-countdown"
#define NOTIFICATION_BUS_NAME            "org.freedesktop.Notifications"
#define NOTIFICATION_BUS_PATH            "/org/freedesktop/Notifications"

#endif /* _CONSTANTS_H_ */
