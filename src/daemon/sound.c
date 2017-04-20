/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007 Jim Ramsay <i.am@jimramsay.com>
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

#include "config.h"

#include "sound.h"

#include <glib/gi18n.h>
#include <canberra-gtk.h>

void
sound_play_file (GtkWidget *widget,
                 const char *filename)
{
        ca_gtk_play_for_widget (widget, MATE_EVENT,
                                CA_PROP_MEDIA_ROLE, "event",
                                CA_PROP_MEDIA_FILENAME, filename,
                                CA_PROP_EVENT_DESCRIPTION, _("Notification"),
                                NULL);
}

