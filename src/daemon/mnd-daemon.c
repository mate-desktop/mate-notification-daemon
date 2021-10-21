/* vi: set sw=4 ts=4 wrap ai: */
/*
 * mnd-daemon.c: This file is part of mate-notification-daemon
 *
 * Copyright (C) 2018 Wu Xiaotian <yetist@gmail.com>
 * Copyright (C) 2018-2021 MATE Developers
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtk.h>
#include <stdlib.h>

#include "daemon.h"

static gboolean debug = FALSE;
static gboolean replace = FALSE;

static GOptionEntry entries[] =
{
	{
		"debug", 0, G_OPTION_FLAG_NONE,
		G_OPTION_ARG_NONE, &debug,
		"Enable debugging code",
		NULL
	},
	{
		"replace", 'r', G_OPTION_FLAG_NONE,
		G_OPTION_ARG_NONE, &replace,
		"Replace a currently running application",
		NULL
	},
	{
		NULL
	}
};

static gboolean parse_arguments (int    *argc, char ***argv)
{
	GOptionContext *context;
	GError *error;

	context = g_option_context_new (NULL);

	g_option_context_add_main_entries (context, entries, NULL);

	error = NULL;
	if (g_option_context_parse (context, argc, argv, &error) == FALSE)
	{
		g_option_context_free (context);
		g_warning ("Failed to parse command line arguments: %s", error->message);
		g_error_free (error);

		return FALSE;
	}

	g_option_context_free (context);

	if (debug)
		g_setenv ("G_MESSAGES_DEBUG", "all", FALSE);

	return TRUE;
}

int main (int argc, char *argv[])
{
	NotifyDaemon *daemon;

	#if defined(HAVE_X11) && defined(HAVE_WAYLAND)
		gdk_set_allowed_backends ("wayland,x11");
	#elif defined(HAVE_WAYLAND)
		gdk_set_allowed_backends ("wayland");
	#else
		gdk_set_allowed_backends ("x11");
	#endif

	gtk_init(&argc, &argv);

	if (!parse_arguments (&argc, &argv))
		return EXIT_FAILURE;

	daemon = notify_daemon_new (replace);

	gtk_main();

	g_object_unref (daemon);

	return EXIT_SUCCESS;
}
