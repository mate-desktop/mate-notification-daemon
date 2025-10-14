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

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "mate-notification-applet-history.h"
#include "../common/constants.h"

#define IMAGE_SIZE 48

static GtkWidget *
create_notification_icon (const gchar *icon_name)
{
  GdkPixbuf *pixbuf = NULL;
  GtkWidget *image;

  if (icon_name && *icon_name) {
    if (g_path_is_absolute (icon_name)) {
      pixbuf = gdk_pixbuf_new_from_file_at_scale (icon_name, IMAGE_SIZE, IMAGE_SIZE, TRUE, NULL);
    } else {
      GtkIconTheme *theme = gtk_icon_theme_get_default ();
      pixbuf = gtk_icon_theme_load_icon (theme, icon_name, IMAGE_SIZE, GTK_ICON_LOOKUP_USE_BUILTIN, NULL);
    }
  }

  if (pixbuf) {
    image = gtk_image_new_from_pixbuf (pixbuf);
    g_object_unref (pixbuf);
  } else {
    image = gtk_image_new_from_icon_name ("mate-notification-properties", GTK_ICON_SIZE_DIALOG);
  }

  gtk_widget_set_valign (image, GTK_ALIGN_CENTER);
  return image;
}

static GtkWidget *
create_popup_window (void)
{
  GtkWidget *popup = gtk_window_new (GTK_WINDOW_POPUP);
  g_object_set (popup,
                "type-hint", GDK_WINDOW_TYPE_HINT_POPUP_MENU,
                "skip-taskbar-hint", TRUE,
                "skip-pager-hint", TRUE,
                "decorated", FALSE,
                "resizable", FALSE,
                NULL);
  gtk_container_set_border_width (GTK_CONTAINER (popup), 1);
  return popup;
}

static void
popup_destroyed_cb (GtkWidget                      *popup,
                    MateNotificationHistoryContext *context)
{
  (void) popup;
  context->history_popup = NULL;
}

static GtkWidget *
create_notification_row (guint id, const gchar *app_name, const gchar *app_icon,
                         const gchar *summary, const gchar *body, gint64 timestamp)
{
  GtkWidget *row, *hbox, *icon_image, *content_box;
  GtkWidget *title_label, *body_label, *time_label;
  GDateTime *dt;
  gchar *time_str, *markup;

  /* Format timestamp */
  dt = g_date_time_new_from_unix_local (timestamp / G_TIME_SPAN_SECOND);
  time_str = g_date_time_format (dt, "%H:%M");
  g_date_time_unref (dt);

  /* Create row container for the entire notification */
  row = gtk_list_box_row_new ();
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 6);

  icon_image = create_notification_icon (app_icon);

  content_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);

  /* Title */
  markup = g_markup_printf_escaped ("<b>%s</b>", summary ? summary : _("(No summary)"));
  title_label = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (title_label), markup);
  gtk_widget_set_halign (title_label, GTK_ALIGN_START);
  gtk_label_set_ellipsize (GTK_LABEL (title_label), PANGO_ELLIPSIZE_END);
  gtk_label_set_max_width_chars (GTK_LABEL (title_label), 40);
  g_free (markup);

  /* Body */
  body_label = NULL;
  if (body && *body) {
    body_label = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (body_label), body);
    gtk_widget_set_halign (body_label, GTK_ALIGN_START);
    gtk_label_set_ellipsize (GTK_LABEL (body_label), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars (GTK_LABEL (body_label), 40);

    /* Set tooltip in case the body overflows */
    gchar *tooltip_text = g_strdup_printf ("%s\n%s", summary, body);
    gtk_widget_set_tooltip_text (row, tooltip_text);
    g_free (tooltip_text);
  }

  /* Time and app label */
  markup = g_markup_printf_escaped ("<small>%s - %s</small>",
                                     app_name ? app_name : _("Unknown"),
                                     time_str);
  time_label = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (time_label), markup);
  gtk_widget_set_halign (time_label, GTK_ALIGN_START);
  g_free (markup);
  g_free (time_str);

  /* Pack content box */
  gtk_box_pack_start (GTK_BOX (content_box), title_label, FALSE, FALSE, 0);
  if (body_label)
    gtk_box_pack_start (GTK_BOX (content_box), body_label, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (content_box), time_label, FALSE, FALSE, 0);

  /* Pack horizontal box */
  gtk_box_pack_start (GTK_BOX (hbox), icon_image, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), content_box, TRUE, TRUE, 0);

  gtk_container_add (GTK_CONTAINER (row), hbox);

  return row;
}

static void
clear_history (GtkWidget                      *button,
               MateNotificationHistoryContext *context)
{
  (void) button;

  /* Clear the notification history */
  if (dbus_context_clear_notification_history (context->dbus_context)) {
    /* Trigger count update after clearing */
    if (context->count_update_callback) {
      ((void (*)(gpointer)) context->count_update_callback) (context->count_update_user_data);
    }
  }

  /* Close the popup */
  if (context->history_popup) {
    gtk_widget_destroy (context->history_popup);
  }
}

static void
dnd_toggle_clicked (GtkToggleButton                *toggle,
                    MateNotificationHistoryContext *context)
{
  gboolean active = gtk_toggle_button_get_active (toggle);

  if (context->settings) {
    g_settings_set_boolean (context->settings, "do-not-disturb", active);
  }
}


MateNotificationHistoryContext *
history_context_new (MateNotificationDBusContext *dbus_context,
                     GtkWidget                   *main_widget,
                     GCallback                    count_update_callback,
                     gpointer                     count_update_user_data,
                     GSettings                   *settings)
{
  MateNotificationHistoryContext *context = g_new0 (MateNotificationHistoryContext, 1);
  context->dbus_context = dbus_context;
  context->main_widget = main_widget;
  context->history_popup = NULL;
  context->count_update_callback = count_update_callback;
  context->count_update_user_data = count_update_user_data;
  context->settings = settings;
  return context;
}

void
history_context_free (MateNotificationHistoryContext *context)
{
  if (context) {
    if (context->history_popup) {
      gtk_widget_destroy (context->history_popup);
    }
    g_free (context);
  }
}

void
history_context_update_dbus (MateNotificationHistoryContext *context,
                             MateNotificationDBusContext    *dbus_context)
{
  if (context) {
    context->dbus_context = dbus_context;
  }
}

static void
position_popup_window (GtkWidget *popup,
                       GtkWidget *reference_widget,
                       GdkScreen *screen,
                       gint popup_width,
                       gint popup_height)
{
  gint x, y;
  GdkWindow *window;

  window = gtk_widget_get_window (reference_widget);
  if (!window) {
    return;
  }

  gdk_window_get_origin (window, &x, &y);

  /* Calculate popup dimensions */
  gint applet_height = gtk_widget_get_allocated_height (reference_widget);

  /* Get screen dimensions */
  gint screen_width = gdk_screen_get_width (screen);
  gint screen_height = gdk_screen_get_height (screen);

  /* Calculate initial position below applet */
  gint popup_x = x;
  gint popup_y = y + applet_height;

  /* Check if popup extends beyond screen boundaries and adjust */
  if (popup_x + popup_width > screen_width) {
    popup_x = screen_width - popup_width;
  }
  if (popup_x < 0) {
    popup_x = 0;
  }

  /* If popup extends below screen, place it above the applet instead */
  if (popup_y + popup_height > screen_height) {
    popup_y = y - popup_height;
    /* If it still doesn't fit above, center it vertically */
    if (popup_y < 0) {
      popup_y = (screen_height - popup_height) / 2;
      if (popup_y < 0) popup_y = 0;
    }
  }

  gtk_window_move (GTK_WINDOW (popup), popup_x, popup_y);
}

void
show_notification_history (MateNotificationHistoryContext *context)
{
  GtkWidget *popup;
  GtkWidget *vbox;
  GtkWidget *scrolled_window;
  GtkWidget *list_box;
  GtkWidget *button_box;
  GtkWidget *dnd_toggle;
  GtkWidget *clear_button;
  GtkWidget *close_button;
  GVariant *result;
  GVariantIter *iter;
  guint id, urgency;
  gchar *app_name, *app_icon, *summary, *body;
  gint64 timestamp, closed_timestamp;
  guint reason;
  gboolean read;

  if (!dbus_context_is_available (context->dbus_context)) {
    g_warning ("Cannot show history: daemon not available");
    return;
  }

  /* Check if history is enabled */
  if (context->settings && !g_settings_get_boolean (context->settings, GSETTINGS_KEY_HISTORY_ENABLED)) {
    g_warning ("Cannot show history: history is disabled for privacy");
    return;
  }

  /* If popup already exists, destroy it (basically toggle off) */
  if (context->history_popup) {
    gtk_widget_destroy (context->history_popup);
    context->history_popup = NULL;
    return;
  }

  /* Get notification history from daemon */
  result = dbus_context_get_notification_history (context->dbus_context);

  if (!result) {
    g_warning ("Failed to get notification history");
    return;
  }

  /* Trigger count update since accessing history marks all as read */
  if (context->count_update_callback) {
    ((void (*)(gpointer)) context->count_update_callback) (context->count_update_user_data);
  }

  popup = create_popup_window ();

  context->history_popup = popup;
  g_signal_connect (popup, "destroy", G_CALLBACK (popup_destroyed_cb), context);

  /* Create main container */
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
  gtk_container_add (GTK_CONTAINER (popup), vbox);

  /* Create list box for all notifications */
  list_box = gtk_list_box_new ();
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (list_box), GTK_SELECTION_NONE);

  /* Get history data and add to list */
  gint notification_count = 0;
  if (result) {
    g_variant_get (result, "(a(ussssxxuub))", &iter);

    while (g_variant_iter_loop (iter, "(ussssxxuub)",
                                &id, &app_name, &app_icon, &summary, &body,
                                &timestamp, &closed_timestamp, &reason, &urgency, &read)) {
      notification_count++;
      /* Add each notification as a new row in the list */
      GtkWidget *row = create_notification_row (id, app_name, app_icon, summary, body, timestamp);
      gtk_list_box_insert (GTK_LIST_BOX (list_box), row, -1);
    }

    g_variant_iter_free (iter);
    g_variant_unref (result);
  }

  /* Add message if list is empty */
  if (gtk_list_box_get_row_at_index (GTK_LIST_BOX (list_box), 0) == NULL) {
    GtkWidget *row = gtk_list_box_row_new ();
    GtkWidget *label = gtk_label_new (_("No notifications"));
    gtk_widget_set_sensitive (label, FALSE);
    gtk_container_set_border_width (GTK_CONTAINER (row), 12);
    gtk_container_add (GTK_CONTAINER (row), label);
    gtk_list_box_insert (GTK_LIST_BOX (list_box), row, -1);
  }

  /* Add this window for the list of notifications */
  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

  /* Calculate to ensure the popup is the right height:
   * 80% of screen height, with some room for buttons */
  GdkScreen *screen = gtk_widget_get_screen (context->main_widget);
  gint max_content_height = (gdk_screen_get_height (screen) * 0.8) - 100;

  gint content_height = 100; /* 100px minimum */
  if (notification_count > 0)
    content_height = MIN(notification_count * content_height, max_content_height);

  /* Now force the scrollable window to be the calculated height */
  gtk_scrolled_window_set_max_content_height (GTK_SCROLLED_WINDOW (scrolled_window), max_content_height);
  gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (scrolled_window), content_height);
  gtk_container_add (GTK_CONTAINER (scrolled_window), list_box);

  gtk_box_pack_start (GTK_BOX (vbox), scrolled_window, TRUE, TRUE, 0);

  /* Add a box for action buttons */
  button_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_container_set_border_width (GTK_CONTAINER (button_box), 6);

  /* DND toggle first */
  dnd_toggle = gtk_check_button_new_with_label (_("Do not disturb"));
  gtk_widget_set_halign (dnd_toggle, GTK_ALIGN_START);

  if (context->settings) {
    gboolean dnd_enabled = g_settings_get_boolean (context->settings, "do-not-disturb");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dnd_toggle), dnd_enabled);
  }

  g_signal_connect (dnd_toggle, "toggled", G_CALLBACK (dnd_toggle_clicked), context);
  gtk_box_pack_start (GTK_BOX (button_box), dnd_toggle, FALSE, FALSE, 0);

  /* Spacer to push buttons to the right */
  GtkWidget *spacer = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_start (GTK_BOX (button_box), spacer, TRUE, TRUE, 0);

  /* Then the action buttons */
  clear_button = gtk_button_new_with_label (_("Clear All"));
  close_button = gtk_button_new_with_label (_("Close"));

  g_signal_connect (clear_button, "clicked", G_CALLBACK (clear_history), context);
  g_signal_connect_swapped (close_button, "clicked", G_CALLBACK (gtk_widget_destroy), popup);

  gtk_box_pack_end (GTK_BOX (button_box), close_button, FALSE, FALSE, 0);
  gtk_box_pack_end (GTK_BOX (button_box), clear_button, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), button_box, FALSE, FALSE, 0);

  /* Position popup window with boundary checking */
  position_popup_window (popup, context->main_widget, screen, 450, content_height + 100);

  /* Set popup size based on content (with space for buttons) */
  gtk_window_set_default_size (GTK_WINDOW (popup), 450, content_height + 100);
  gtk_widget_show_all (popup);
}
