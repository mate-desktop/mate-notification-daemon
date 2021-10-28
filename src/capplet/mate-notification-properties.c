/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007 Jonh Wendell <wendell@bani.com.br>
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

#include <glib/gi18n.h>
#include <glib.h>
#include <gmodule.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gio/gio.h>
#include <string.h>
#include <libnotify/notify.h>

#include "stack.h"
#include "constants.h"

typedef struct {
	GSettings* gsettings;

	GtkWidget* dialog;
	GtkWidget* position_combo;
	GtkWidget* monitor_combo;
	GtkWidget* theme_combo;
	GtkWidget* preview_button;
	GtkWidget* active_checkbox;
	GtkWidget* dnd_checkbox;
	GtkWidget* monitor_label;

	NotifyNotification* preview;
} NotificationAppletDialog;

enum {
	NOTIFY_POSITION_LABEL,
	NOTIFY_POSITION_NAME,
	N_COLUMNS_POSITION
};

enum {
    NOTIFY_MONITOR_NUMBER,
    N_COLUMNS_MONITOR
};

enum {
	NOTIFY_THEME_LABEL,
	NOTIFY_THEME_NAME,
	NOTIFY_THEME_FILENAME,
	N_COLUMNS_THEME
};

static void notification_properties_position_notify(GSettings *settings, gchar *key, NotificationAppletDialog* dialog)
{
	GtkTreeModel* model;
	GtkTreeIter iter;
	char *location;
	gboolean valid;

	location = g_settings_get_string(dialog->gsettings, key);

	model = gtk_combo_box_get_model(GTK_COMBO_BOX(dialog->position_combo));
	valid = gtk_tree_model_get_iter_first(model, &iter);

	for (valid = gtk_tree_model_get_iter_first(model, &iter); valid; valid = gtk_tree_model_iter_next(model, &iter))
	{
		gchar *it_key;

		gtk_tree_model_get(model, &iter, NOTIFY_POSITION_NAME, &it_key, -1);

		if (g_str_equal(it_key, location))
		{
			gtk_combo_box_set_active_iter(GTK_COMBO_BOX(dialog->position_combo), &iter);
			g_free(it_key);
			break;
		}
		g_free(it_key);
	}

	g_free(location);
}

static void notification_properties_monitor_changed(GtkComboBox* widget, NotificationAppletDialog* dialog)
{
	gint monitor;
	GtkTreeIter iter;

	GtkTreeModel *model = gtk_combo_box_get_model(GTK_COMBO_BOX(dialog->monitor_combo));

	if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(dialog->monitor_combo), &iter))
	{
		return;
	}

	gtk_tree_model_get(model, &iter, NOTIFY_MONITOR_NUMBER, &monitor, -1);

	g_settings_set_int(dialog->gsettings, GSETTINGS_KEY_MONITOR_NUMBER, monitor);
}

static void notification_properties_monitor_notify(GSettings *settings, gchar *key, NotificationAppletDialog* dialog)
{
	GtkTreeModel* model;
	GtkTreeIter iter;
	gint monitor_number;
	gint monitor_number_at_iter;
	gboolean valid;

	model = gtk_combo_box_get_model(GTK_COMBO_BOX(dialog->monitor_combo));

	monitor_number = g_settings_get_int(dialog->gsettings, GSETTINGS_KEY_MONITOR_NUMBER);

	for (valid = gtk_tree_model_get_iter_first(model, &iter); valid; valid = gtk_tree_model_iter_next(model, &iter))
	{
		gtk_tree_model_get(model, &iter, NOTIFY_MONITOR_NUMBER, &monitor_number_at_iter, -1);

		if (monitor_number_at_iter == monitor_number)
		{
			gtk_combo_box_set_active_iter(GTK_COMBO_BOX(dialog->monitor_combo), &iter);
			break;
		}
	}
}

static void notification_properties_location_changed(GtkComboBox* widget, NotificationAppletDialog* dialog)
{
	char* location;
	GtkTreeIter iter;

	GtkTreeModel* model = gtk_combo_box_get_model(GTK_COMBO_BOX(dialog->position_combo));

	if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(dialog->position_combo), &iter))
	{
		return;
	}

	gtk_tree_model_get(model, &iter, NOTIFY_POSITION_NAME, &location, -1);

	g_settings_set_string (dialog->gsettings, GSETTINGS_KEY_POPUP_LOCATION, location);
	g_free(location);
}

static void notification_properties_dialog_setup_positions(NotificationAppletDialog* dialog)
{
	char* location;
	gboolean valid;
	GtkTreeModel* model;
	GtkTreeIter iter;

	g_signal_connect(dialog->gsettings, "changed::" GSETTINGS_KEY_POPUP_LOCATION, G_CALLBACK (notification_properties_position_notify), dialog);
	g_signal_connect(dialog->position_combo, "changed", G_CALLBACK(notification_properties_location_changed), dialog);

	model = gtk_combo_box_get_model(GTK_COMBO_BOX(dialog->position_combo));
	location = g_settings_get_string(dialog->gsettings, GSETTINGS_KEY_POPUP_LOCATION);

	for (valid = gtk_tree_model_get_iter_first(model, &iter); valid; valid = gtk_tree_model_iter_next(model, &iter))
	{
		gchar* key;

		gtk_tree_model_get(model, &iter, NOTIFY_POSITION_NAME, &key, -1);

		if (g_str_equal(key, location))
		{
			gtk_combo_box_set_active_iter(GTK_COMBO_BOX(dialog->position_combo), &iter);
			g_free(key);
			break;
		}

		g_free(key);
	}

	g_free(location);
}

static void notification_properties_dialog_setup_monitors(NotificationAppletDialog* dialog)
{
	GtkListStore *store;
	GdkDisplay *display;
	GdkScreen *screen;
	GtkTreeIter iter;
	gint num_monitors;
	gint cur_monitor_number;
	gint cur_monitor_number_at_iter;
	gboolean valid;

	// Assumes the user has only one display.
	// TODO: add support for multiple displays.
	display = gdk_display_get_default();
	g_assert(display != NULL);

	// Assumes the user has only one screen.
	// TODO: add support for mulitple screens.
	screen = gdk_display_get_default_screen(display);
	g_assert(screen != NULL);

	num_monitors = gdk_display_get_n_monitors(display);
	g_assert(num_monitors >= 1);

	store = gtk_list_store_new(N_COLUMNS_MONITOR, G_TYPE_INT);

    gint i;
	for (i = 0; i < num_monitors; i++)
	{
        gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, NOTIFY_MONITOR_NUMBER, i, -1);
	}

	gtk_combo_box_set_model(GTK_COMBO_BOX (dialog->monitor_combo), GTK_TREE_MODEL (store));

	g_signal_connect(dialog->gsettings, "changed::" GSETTINGS_KEY_MONITOR_NUMBER, G_CALLBACK (notification_properties_monitor_notify), dialog);
	cur_monitor_number = g_settings_get_int(dialog->gsettings, GSETTINGS_KEY_MONITOR_NUMBER);

	for (valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL (store), &iter); valid; valid = gtk_tree_model_iter_next(GTK_TREE_MODEL (store), &iter))
	{
		gtk_tree_model_get(GTK_TREE_MODEL (store), &iter, NOTIFY_MONITOR_NUMBER, &cur_monitor_number_at_iter, -1);

		if (cur_monitor_number_at_iter == cur_monitor_number)
		{
			gtk_combo_box_set_active_iter(GTK_COMBO_BOX(dialog->monitor_combo), &iter);
			break;
		}
	}

	g_object_unref(store);

	g_signal_connect(dialog->monitor_combo, "changed", G_CALLBACK(notification_properties_monitor_changed), dialog);
}

static void notification_properties_theme_notify(GSettings *settings, gchar *key, NotificationAppletDialog* dialog)
{
	char* theme = g_settings_get_string(dialog->gsettings, key);

	GtkTreeModel* model = gtk_combo_box_get_model(GTK_COMBO_BOX(dialog->theme_combo));

	GtkTreeIter iter;
	gboolean valid;

	for (valid = gtk_tree_model_get_iter_first(model, &iter); valid; valid = gtk_tree_model_iter_next(model, &iter))
	{
		gchar* theme_name;

		gtk_tree_model_get(model, &iter, NOTIFY_THEME_NAME, &theme_name, -1);

		if (g_str_equal(theme_name, theme))
		{
			gtk_combo_box_set_active_iter(GTK_COMBO_BOX(dialog->theme_combo), &iter);
			g_free(theme_name);
			break;
		}

		g_free(theme_name);
	}

	g_free(theme);
}

static void notification_properties_theme_changed(GtkComboBox* widget, NotificationAppletDialog* dialog)
{
	char* theme;
	GtkTreeIter iter;

	GtkTreeModel* model = gtk_combo_box_get_model(GTK_COMBO_BOX(dialog->theme_combo));

	if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(dialog->theme_combo), &iter))
	{
		return;
	}

	gtk_tree_model_get(model, &iter, NOTIFY_THEME_NAME, &theme, -1);
	g_settings_set_string(dialog->gsettings, GSETTINGS_KEY_THEME, theme);
	g_free(theme);
}

static gchar* get_theme_name(const gchar* filename)
{
	/* TODO: Remove magic numbers. Strip "lib" and ".so" */
	gchar* result = g_strdup(filename + 3);
	result[strlen(result) - strlen("." G_MODULE_SUFFIX)] = '\0';
	return result;
}

static void notification_properties_dialog_setup_themes(NotificationAppletDialog* dialog)
{
	GDir* dir;
	const gchar* filename;
	char* theme;
	char* theme_name;
	char* theme_label;
	gboolean valid;
	GtkListStore* store;
	GtkTreeIter iter;

	store = gtk_list_store_new(N_COLUMNS_THEME, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

	gtk_combo_box_set_model(GTK_COMBO_BOX(dialog->theme_combo), GTK_TREE_MODEL(store));

	g_signal_connect(dialog->gsettings, "changed::" GSETTINGS_KEY_THEME, G_CALLBACK (notification_properties_theme_notify), dialog);
	g_signal_connect(dialog->theme_combo, "changed", G_CALLBACK(notification_properties_theme_changed), dialog);

	if ((dir = g_dir_open(ENGINES_DIR, 0, NULL)))
	{
		while ((filename = g_dir_read_name(dir)))
		{
			if (g_str_has_suffix(filename, "." G_MODULE_SUFFIX))
			{
				theme_name = get_theme_name(filename);

				/* FIXME: other solution than hardcode? */
				if (g_str_equal(theme_name, "coco"))
				{
					theme_label = g_strdup(_("Coco"));
				}
				else if (g_str_equal(theme_name, "nodoka"))
				{
					theme_label = g_strdup(_("Nodoka"));
				}
				else if (g_str_equal(theme_name, "slider"))
				{
					theme_label = g_strdup(_("Slider"));
				}
				else if (g_str_equal(theme_name, "standard"))
				{
					theme_label = g_strdup(_("Standard theme"));
				}
				else
				{
					theme_label = g_strdup(theme_name);
				}

				gtk_list_store_append(store, &iter);
				gtk_list_store_set(store, &iter, NOTIFY_THEME_LABEL, theme_label, NOTIFY_THEME_NAME, theme_name, NOTIFY_THEME_FILENAME, filename, -1);
				g_free(theme_name);
				g_free(theme_label);
			}
		}

		g_dir_close(dir);
	}
	else
	{
		g_warning("Error opening themes dir %s", ENGINES_DIR);
	}

	theme = g_settings_get_string(dialog->gsettings, GSETTINGS_KEY_THEME);

	for (valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter); valid; valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter))
	{
		gchar* key;

		gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, NOTIFY_THEME_NAME, &key, -1);

		if (g_str_equal(key, theme))
		{
			gtk_combo_box_set_active_iter(GTK_COMBO_BOX(dialog->theme_combo), &iter);
			g_free(key);
			break;
		}

		g_free(key);
	}

	g_free(theme);
}

static void show_message(NotificationAppletDialog* dialog, const gchar* message)
{
	GtkWidget* d = gtk_message_dialog_new(GTK_WINDOW(dialog->dialog), GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "%s", message);
	gtk_dialog_run(GTK_DIALOG(d));
	gtk_widget_destroy(d);
}

static void notification_properties_dialog_preview_closed(NotifyNotification* preview, NotificationAppletDialog* dialog)
{
	if (preview == dialog->preview)
	{
		dialog->preview = NULL;
	}

	g_object_unref(preview);
}

static void notification_properties_dialog_preview(NotificationAppletDialog* dialog)
{
	if (!notify_is_initted() && !notify_init("n-d"))
	{
		show_message(dialog, _("Error initializing libmatenotify"));
		return;
	}

	GError* error = NULL;

	if (dialog->preview)
	{
		notify_notification_close(dialog->preview, NULL);
		g_object_unref(dialog->preview);
		dialog->preview = NULL;
	}

	dialog->preview = notify_notification_new(_("Notification Test"), _("Just a test"), "dialog-information");

	if (!notify_notification_show(dialog->preview, &error))
	{
		char* message = g_strdup_printf(_("Error while displaying notification: %s"), error->message);
		show_message(dialog, message);
		g_error_free(error);
		g_free(message);
	}

	g_signal_connect(dialog->preview, "closed", G_CALLBACK(notification_properties_dialog_preview_closed), dialog);
}

static void notification_properties_dialog_response(GtkWidget* widget, int response, NotificationAppletDialog* dialog)
{
	switch (response)
	{
		case GTK_RESPONSE_ACCEPT:
			notification_properties_dialog_preview(dialog);
			break;

		case GTK_RESPONSE_CLOSE:
		default:
			gtk_widget_destroy(widget);
			break;
	}
}

static void notification_properties_dialog_destroyed(GtkWidget* widget, NotificationAppletDialog* dialog)
{
	dialog->dialog = NULL;

	gtk_main_quit();
}

static gboolean notification_properties_dialog_init(NotificationAppletDialog* dialog)
{
	GtkBuilder* builder = gtk_builder_new();
	GError* error = NULL;
	gboolean inv_active_checkbox;

	gtk_builder_add_from_resource (builder, "/org/mate/notifications/properties/mate-notification-properties.ui", &error);
	if (error != NULL)
	{
		g_warning(_("Could not load user interface: %s"), error->message);
		g_error_free(error);
		return FALSE;
	}

	dialog->dialog = GTK_WIDGET(gtk_builder_get_object(builder, "dialog"));
	dialog->position_combo = GTK_WIDGET(gtk_builder_get_object(builder, "position_combo"));
	dialog->monitor_combo = GTK_WIDGET(gtk_builder_get_object(builder, "monitor_combo"));
	dialog->theme_combo = GTK_WIDGET(gtk_builder_get_object(builder, "theme_combo"));
	dialog->active_checkbox = GTK_WIDGET(gtk_builder_get_object(builder, "use_active_check"));
	dialog->dnd_checkbox = GTK_WIDGET(gtk_builder_get_object(builder, "do_not_disturb_check"));
	dialog->monitor_label = GTK_WIDGET(gtk_builder_get_object(builder, "monitor_label"));

	g_object_unref (builder);

	dialog->gsettings = g_settings_new (GSETTINGS_SCHEMA);

	g_signal_connect (dialog->dialog, "response", G_CALLBACK(notification_properties_dialog_response), dialog);
	g_signal_connect (dialog->dialog, "destroy", G_CALLBACK(notification_properties_dialog_destroyed), dialog);

	g_settings_bind (dialog->gsettings, GSETTINGS_KEY_USE_ACTIVE_MONITOR, dialog->active_checkbox, "active", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (dialog->gsettings, GSETTINGS_KEY_DO_NOT_DISTURB, dialog->dnd_checkbox, "active", G_SETTINGS_BIND_DEFAULT);

	notification_properties_dialog_setup_themes (dialog);
	notification_properties_dialog_setup_positions (dialog);
	notification_properties_dialog_setup_monitors (dialog);

	inv_active_checkbox = !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->active_checkbox));
	gtk_widget_set_sensitive (dialog->monitor_combo, inv_active_checkbox);
	gtk_widget_set_sensitive (dialog->monitor_label, inv_active_checkbox);
	g_object_bind_property (dialog->active_checkbox, "active", dialog->monitor_combo, "sensitive", G_BINDING_INVERT_BOOLEAN);
	g_object_bind_property (dialog->active_checkbox, "active", dialog->monitor_label, "sensitive", G_BINDING_INVERT_BOOLEAN);

	gtk_widget_show_all(dialog->dialog);

	dialog->preview = NULL;

	return TRUE;
}

static void notification_properties_dialog_finalize(NotificationAppletDialog* dialog)
{
	if (dialog->dialog != NULL)
	{
		gtk_widget_destroy(dialog->dialog);
		dialog->dialog = NULL;
	}

	if (dialog->preview)
	{
		notify_notification_close(dialog->preview, NULL);
		dialog->preview = NULL;
	}
	g_free (dialog);
}

int main(int argc, char** argv)
{
	NotificationAppletDialog *dialog;

#ifdef ENABLE_NLS
	bindtextdomain(GETTEXT_PACKAGE, NOTIFICATION_LOCALEDIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);
#endif /* ENABLE_NLS */

	gtk_init(&argc, &argv);

	notify_init("mate-notification-properties");

	dialog = g_new0 (NotificationAppletDialog, 1);
	if (!notification_properties_dialog_init (dialog))
	{
		notification_properties_dialog_finalize (dialog);
		return 1;
	}

	gtk_main();

	notification_properties_dialog_finalize (dialog);

	return 0;
}
