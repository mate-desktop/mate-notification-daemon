/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2020 William Wold <wm@wmww.sh>
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

#ifndef HAVE_WAYLAND
#error file should only be built when HAVE_WAYLAND is enabled
#endif

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#include <gtk-layer-shell/gtk-layer-shell.h>

#include "wayland.h"

void wayland_init_notification (GtkWindow* nw)
{
	gtk_layer_init_for_window (nw);
}

void wayland_move_notification (GtkWindow* nw, int x, int y)
{
	GdkWindow *window = gtk_widget_get_window (GTK_WIDGET (nw));
	GdkMonitor *monitor = gdk_display_get_monitor_at_window (
		gdk_window_get_display (window),
		window);
	GdkRectangle workarea;
	gdk_monitor_get_workarea (monitor, &workarea);
	GtkRequisition  req;
	gtk_widget_get_preferred_size (GTK_WIDGET (nw), NULL, &req);
	int left_gap = x;
	int top_gap = y;
	int right_gap = workarea.width - x - req.width;
	int bottom_gap = workarea.height - y - req.height;

	if (left_gap < right_gap)
	{
		gtk_layer_set_anchor (nw, GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
		gtk_layer_set_anchor (nw, GTK_LAYER_SHELL_EDGE_RIGHT, FALSE);
		gtk_layer_set_margin (nw, GTK_LAYER_SHELL_EDGE_LEFT, left_gap);
	}
	else
	{
		gtk_layer_set_anchor (nw, GTK_LAYER_SHELL_EDGE_LEFT, FALSE);
		gtk_layer_set_anchor (nw, GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
		gtk_layer_set_margin (nw, GTK_LAYER_SHELL_EDGE_RIGHT, right_gap);
	}

	if (top_gap < bottom_gap)
	{
		gtk_layer_set_anchor (nw, GTK_LAYER_SHELL_EDGE_TOP, TRUE);
		gtk_layer_set_anchor (nw, GTK_LAYER_SHELL_EDGE_BOTTOM, FALSE);
		gtk_layer_set_margin (nw, GTK_LAYER_SHELL_EDGE_TOP, top_gap);
	}
	else
	{
		gtk_layer_set_anchor (nw, GTK_LAYER_SHELL_EDGE_TOP, FALSE);
		gtk_layer_set_anchor (nw, GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
		gtk_layer_set_margin (nw, GTK_LAYER_SHELL_EDGE_BOTTOM, bottom_gap);
	}
}
