/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2020 William Wold <wm@wmww.sh>
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

#ifndef _WAYLAND_H
#define _WAYLAND_H

#ifdef PACKAGE_NAME /* only check HAVE_WAYLAND if config.h has been included */
#ifndef HAVE_WAYLAND
#error file should only be included when HAVE_WAYLAND is enabled
#endif
#endif

#include <gtk/gtk.h>

void wayland_init_notification (GtkWindow* nw);
void wayland_move_notification (GtkWindow* nw, int x, int y);

#endif /* _WAYLAND_H */
