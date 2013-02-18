/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007 Jonh Wendell <wendell@bani.com.br>
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

#include <glib/gi18n.h>
#include <glib.h>
#include <gmodule.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gio/gio.h>
#include <string.h>
#include <libnotify/notify.h>

#include "stack.h"

#define GSETTINGS_SCHEMA "org.mate.NotificationDaemon"
#define GSETTINGS_KEY_THEME "theme"
#define GSETTINGS_KEY_POPUP_LOCATION "popup-location"
#define GSETTINGS_KEY_MONITOR_NUMBER "monitor-number"
#define GSETTINGS_KEY_USE_ACTIVE_MONITOR "use-active-monitor"

#define NOTIFICATION_UI_FILE "mate-notification-properties.ui"

typedef struct {
	GSettings* gsettings;

	GtkWidget* dialog;
	GtkWidget* position_combo;
    GtkWidget* monitor_combo;
	GtkWidget* theme_combo;
	GtkWidget* preview_button;
	GtkWidget* active_checkbox;
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
	const char* location;
	gboolean valid;

	location = g_settings_get_string(dialog->gsettings, key);

	model = gtk_combo_box_get_model(GTK_COMBO_BOX(dialog->position_combo));
	valid = gtk_tree_model_get_iter_first(model, &iter);

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

	model = gtk_combo_box_get_model(GTK_COMBO_BOX(dialog->position_combo));
	g_signal_connect(dialog->position_combo, "changed", G_CALLBACK(notification_properties_location_changed), dialog);

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

	g_signal_connect (dialog->gsettings, "changed::" GSETTINGS_KEY_POPUP_LOCATION, G_CALLBACK (notification_properties_position_notify), dialog);
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
	
	num_monitors = gdk_screen_get_n_monitors(screen);
	g_assert(num_monitors >= 1);
	
	store = gtk_list_store_new(N_COLUMNS_MONITOR, G_TYPE_INT);

    gint i;
	for (i = 0; i < num_monitors; i++)
	{
        gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, NOTIFY_MONITOR_NUMBER, i, -1);
	}
	
	gtk_combo_box_set_model(GTK_COMBO_BOX (dialog->monitor_combo), GTK_TREE_MODEL (store));
	
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
	g_signal_connect(dialog->gsettings, "changed::" GSETTINGS_KEY_MONITOR_NUMBER, G_CALLBACK (notification_properties_monitor_notify), dialog);
}

static void notification_properties_theme_notify(GSettings *settings, gchar *key, NotificationAppletDialog* dialog)
{
	const char* theme = g_settings_get_string(dialog->gsettings, key);

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
		g_warning("Error opening themes dir");
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

	g_signal_connect (dialog->gsettings, "changed::" GSETTINGS_KEY_THEME, G_CALLBACK (notification_properties_theme_notify), dialog);
	g_free(theme);
}

static void notification_properties_checkbox_toggled(GtkWidget* widget, NotificationAppletDialog* dialog)
{
    gboolean is_active;

    is_active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (widget));

    // This was called as a result of notification_properties_checkbox_notify being called.
    // Stop here instead of doing redundant work.
    if (is_active == g_settings_get_boolean(dialog->gsettings, GSETTINGS_KEY_USE_ACTIVE_MONITOR))
    {
        return;
    }

	if (is_active)
	{
		g_settings_set_boolean(dialog->gsettings, GSETTINGS_KEY_USE_ACTIVE_MONITOR, TRUE);
		gtk_widget_set_sensitive(dialog->monitor_combo, FALSE);
		gtk_widget_set_sensitive(dialog->monitor_label, FALSE);
	}
	else
	{
		g_settings_set_boolean(dialog->gsettings, GSETTINGS_KEY_USE_ACTIVE_MONITOR, FALSE);
		gtk_widget_set_sensitive(dialog->monitor_combo, TRUE);
		gtk_widget_set_sensitive(dialog->monitor_label, TRUE);
	}
}

static void notification_properties_checkbox_notify(GSettings *settings, gchar *key, NotificationAppletDialog* dialog)
{
    gboolean is_set;

    is_set = g_settings_get_boolean(settings, key);

    // This was called as a result of notification_properties_checkbox_toggled being called.
    // Stop here instead of doing redundant work.
    if(is_set == gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (dialog->active_checkbox)))
    {
        return;
    }

	if (is_set)
	{
		gtk_widget_set_sensitive(dialog->monitor_combo, FALSE);
		gtk_widget_set_sensitive(dialog->monitor_label, FALSE);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (dialog->active_checkbox), TRUE);
	}
	else
	{
		gtk_widget_set_sensitive(dialog->monitor_combo, TRUE);
		gtk_widget_set_sensitive(dialog->monitor_label, TRUE);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (dialog->active_checkbox), FALSE);
	}
}

static void notification_properties_dialog_help(void)
{
	/* Do nothing */
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
		case GTK_RESPONSE_HELP:
			notification_properties_dialog_help();
			break;

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
	const char* ui_file;

	if (g_file_test(NOTIFICATION_UI_FILE, G_FILE_TEST_EXISTS))
	{
		ui_file = NOTIFICATION_UI_FILE;
	}
	else
	{
		ui_file = NOTIFICATION_UIDIR "/" NOTIFICATION_UI_FILE;
	}

	GtkBuilder* builder = gtk_builder_new();
	GError* error = NULL;

	gtk_builder_add_from_file(builder, ui_file, &error);

	if (error != NULL)
	{
		g_warning(_("Could not load user interface file: %s"), error->message);
		g_error_free(error);
		return FALSE;
	}

	dialog->dialog = GTK_WIDGET(gtk_builder_get_object(builder, "dialog"));
	g_assert(dialog->dialog != NULL);

	dialog->position_combo = GTK_WIDGET(gtk_builder_get_object(builder, "position_combo"));
	g_assert(dialog->position_combo != NULL);

	dialog->monitor_combo = GTK_WIDGET(gtk_builder_get_object(builder, "monitor_combo"));
	g_assert(dialog->monitor_combo != NULL);
	
	dialog->theme_combo = GTK_WIDGET(gtk_builder_get_object(builder, "theme_combo"));
	g_assert(dialog->theme_combo != NULL);
	
	dialog->active_checkbox = GTK_WIDGET(gtk_builder_get_object(builder, "use_active_check"));
	g_assert(dialog->active_checkbox != NULL);	

    dialog->monitor_label = GTK_WIDGET(gtk_builder_get_object(builder, "monitor_label"));
    g_assert(dialog->monitor_label != NULL);
	
	g_object_unref(builder);

	dialog->gsettings = g_settings_new (GSETTINGS_SCHEMA);

	g_signal_connect(dialog->dialog, "response", G_CALLBACK(notification_properties_dialog_response), dialog);
	g_signal_connect(dialog->dialog, "destroy", G_CALLBACK(notification_properties_dialog_destroyed), dialog);
	g_signal_connect(dialog->active_checkbox, "toggled", G_CALLBACK(notification_properties_checkbox_toggled), dialog);
	g_signal_connect (dialog->gsettings, "changed::" GSETTINGS_KEY_USE_ACTIVE_MONITOR, G_CALLBACK (notification_properties_checkbox_notify), dialog);

	notification_properties_dialog_setup_themes(dialog);
	notification_properties_dialog_setup_positions(dialog);
	notification_properties_dialog_setup_monitors(dialog);

	if (g_settings_get_boolean(dialog->gsettings, GSETTINGS_KEY_USE_ACTIVE_MONITOR))
	{
		gtk_widget_set_sensitive(dialog->monitor_combo, FALSE);
		gtk_widget_set_sensitive(dialog->monitor_label, FALSE);
	}
	else
	{
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (dialog->active_checkbox), FALSE);
        gtk_widget_set_sensitive(dialog->monitor_combo, TRUE);
        gtk_widget_set_sensitive(dialog->monitor_label, TRUE);
	}

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
}

int main(int argc, char** argv)
{
	NotificationAppletDialog dialog = {NULL, }; /* <- ? */

	bindtextdomain(GETTEXT_PACKAGE, NOTIFICATION_LOCALEDIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);

	gtk_init(&argc, &argv);

	notify_init("mate-notification-properties");

	if (!notification_properties_dialog_init(&dialog))
	{
		notification_properties_dialog_finalize(&dialog);
		return 1;
	}

	gtk_main();

	notification_properties_dialog_finalize(&dialog);

	return 0;
}
