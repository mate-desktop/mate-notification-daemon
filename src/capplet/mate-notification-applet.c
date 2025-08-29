/* mate-notification-applet.c - MATE Notification Applet
 *
 * Copyright (c) 2020 Robert Buj <robert.buj@gmail.com>
 * Copyright (C) 2020-2021 MATE Developers
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

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <mate-panel-applet.h>
#include <gio/gio.h>

#define MATE_DESKTOP_USE_UNSTABLE_API
#include <libmate-desktop/mate-desktop-utils.h>

#include "constants.h"
#include "mate-notification-applet-dbus.h"
#include "mate-notification-applet-history.h"

typedef struct
{
  MatePanelApplet *applet;

  GtkWidget       *status_image;
  GtkWidget       *overlay;
  GtkWidget       *count_label;
  GtkActionGroup  *action_group;
  GSettings       *settings;
  guint            unread_count;
  guint            update_timer_id;

  MateNotificationDBusContext    *dbus_context;
  MateNotificationHistoryContext *history_context;
} MateNotificationApplet;

static void
show_about      (GtkAction              *action,
                 MateNotificationApplet *applet);
static void
call_properties (GtkAction              *action,
                 MateNotificationApplet *applet);
static void
call_show_history (GtkAction              *action,
                   MateNotificationApplet *applet);
void
call_clear_history (GtkAction              *action,
                    MateNotificationApplet *applet);
static void
call_mark_all_read (GtkAction              *action,
                    MateNotificationApplet *applet);

/* D-Bus and notification history functions */
static void
setup_daemon_connection (MateNotificationApplet *applet);
static void
update_unread_count     (MateNotificationApplet *applet);
static void
update_applet_display   (MateNotificationApplet *applet);
static void
update_count_badge      (MateNotificationApplet *applet);
static gboolean
periodic_update_count   (MateNotificationApplet *applet);

/* Click handler functions */
static gboolean
applet_button_press_cb  (GtkWidget              *widget,
                         GdkEventButton         *event,
                         MateNotificationApplet *applet);
static void
toggle_do_not_disturb   (MateNotificationApplet *applet);

static const GtkActionEntry applet_menu_actions [] = {
  { "ShowHistory", "document-open-recent", N_("_Show History"),
    NULL, NULL, G_CALLBACK (call_show_history) },
  { "ClearHistory", "edit-clear", N_("_Clear History"),
    NULL, NULL, G_CALLBACK (call_clear_history) },
  { "MarkAllRead", "edit-select-all", N_("_Mark All as Read"),
    NULL, NULL, G_CALLBACK (call_mark_all_read) },
  { "Preferences", "document-properties", N_("_Preferences"),
    NULL, NULL, G_CALLBACK (call_properties) },
  { "About", "help-about", N_("_About"),
    NULL, NULL, G_CALLBACK (show_about) }
};

static const char* notification_properties[] = {
  "mate-notification-properties",
};

static void
applet_destroy (MatePanelApplet *applet_widget,
                MateNotificationApplet *applet)
{
  g_assert (applet);

  if (applet->update_timer_id > 0)
    g_source_remove (applet->update_timer_id);

  if (applet->dbus_context) {
    dbus_context_free (applet->dbus_context);
  }

  if (applet->history_context) {
    history_context_free (applet->history_context);
  }

  g_object_unref (applet->settings);
  g_object_unref (applet->action_group);
  g_free (applet);
}

static void
call_show_history (GtkAction              *action,
                   MateNotificationApplet *applet)
{
  (void) action;

  if (applet->history_context)
    show_notification_history (applet->history_context);
}

void
call_clear_history (GtkAction              *action,
                    MateNotificationApplet *applet)
{
  (void) action;

  if (dbus_context_clear_notification_history (applet->dbus_context))
    update_unread_count (applet);
}

static void
call_mark_all_read (GtkAction              *action,
                    MateNotificationApplet *applet)
{
  (void) action;

  /* Update count after marking all as read */
  if (dbus_context_mark_all_notifications_as_read (applet->dbus_context))
    update_unread_count (applet);
}

static void
call_properties (GtkAction              *action,
                 MateNotificationApplet *applet)
{
  gsize i;
  (void) action;

  for (i = 0; i < G_N_ELEMENTS (notification_properties); i++) {
    char *programpath = g_find_program_in_path (notification_properties[i]);

    if (programpath != NULL) {
      g_free (programpath);
      mate_gdk_spawn_command_line_on_screen (gtk_widget_get_screen (GTK_WIDGET (applet->applet)),
                                             notification_properties[i],
                                             NULL);
      return;
    }
  }
}

static void
show_about (GtkAction              *action,
            MateNotificationApplet *applet)
{
  static const char *authors[] = {
    "MATE Developers",
    "Robert Buj <robert.buj@gmail.com>",
    NULL
  };
  (void) action;
  (void) applet;

  gtk_show_about_dialog (NULL,
                         "title", _("About Notification Status"),
                         "version", VERSION,
                         "copyright", _("Copyright \xc2\xa9 2021 MATE developers"),
                         "comments", _("Monitor and control notification status."),
                         "authors", authors,
                         "translator-credits", _("translator-credits"),
                         "logo_icon_name", "mate-notification-properties",
                         NULL);
}

static void
set_status_image (MateNotificationApplet *applet,
                  gboolean                dnd_active,
                  gboolean                history_enabled)
{
  const char *icon_name;
  gint size, scale;
  cairo_surface_t *surface;

  if (dnd_active && history_enabled) {
    icon_name = "user-busy";
  } else if (dnd_active && !history_enabled) {
    icon_name = "user-offline";
  } else if (!dnd_active && !history_enabled) {
    icon_name = "user-invisible";
  } else {
    icon_name = "user-available";
  }

  size = (gint) mate_panel_applet_get_size (applet->applet);
  scale = gtk_widget_get_scale_factor (GTK_WIDGET (applet->applet));

  surface = gtk_icon_theme_load_surface (gtk_icon_theme_get_default (),
                                         icon_name,
                                         size, scale,
                                         NULL, 0, NULL);
  if (surface) {
    gtk_image_set_from_surface (GTK_IMAGE (applet->status_image), surface);
    cairo_surface_destroy (surface);
  }

  gtk_widget_show_all (GTK_WIDGET (applet->applet));
}

static void
settings_changed (GSettings              *settings,
                  gchar                  *key,
                  MateNotificationApplet *applet)
{
  if (g_strcmp0 (GSETTINGS_KEY_DO_NOT_DISTURB, key) == 0 ||
      g_strcmp0 (GSETTINGS_KEY_HISTORY_ENABLED, key) == 0)
    update_applet_display (applet);
}

static void
applet_size_changed (MatePanelApplet *applet_widget,
                     int arg1,
                     MateNotificationApplet *applet)
{
  update_applet_display (applet);
}

static void
setup_daemon_connection (MateNotificationApplet *applet)
{
  if (dbus_context_connect (applet->dbus_context)) {
    update_unread_count (applet);
  } else {
    applet->unread_count = 0;
  }

  /* Update history context with new daemon connection */
  if (applet->history_context) {
    history_context_update_dbus (applet->history_context, applet->dbus_context);
  }
}

static void
update_unread_count (MateNotificationApplet *applet)
{
  applet->unread_count = dbus_context_get_notification_count (applet->dbus_context);
  update_applet_display (applet);
}

static void
update_applet_display (MateNotificationApplet *applet)
{
  gchar *tooltip_text;
  gboolean dnd_active;
  gboolean history_enabled;

  dnd_active = g_settings_get_boolean (applet->settings, GSETTINGS_KEY_DO_NOT_DISTURB);
  history_enabled = g_settings_get_boolean (applet->settings, GSETTINGS_KEY_HISTORY_ENABLED);

  /* Update tooltip based on number of unread notifications and user state */
  if (!history_enabled)
    tooltip_text = g_strdup (_("(privacy mode)"));
  else if (applet->unread_count > 99)
    tooltip_text = g_strdup (_("(99+ unread)"));
  else if (applet->unread_count > 0)
    tooltip_text = g_strdup_printf ("(%d unread)", applet->unread_count);
  else
    tooltip_text = g_strdup ("");

  if (dnd_active)
    tooltip_text = g_strdup_printf ("Do Not Disturb %s", tooltip_text);
  else if (!dbus_context_is_available (applet->dbus_context))
    tooltip_text = g_strdup (_("Notifications (daemon unavailable)"));
  else
    tooltip_text = g_strdup_printf ("Notifications %s", tooltip_text);

  gtk_widget_set_tooltip_text (GTK_WIDGET (applet->applet), tooltip_text);
  g_free (tooltip_text);

  set_status_image (applet, dnd_active, history_enabled);
  update_count_badge (applet);
}

static gboolean
applet_button_press_cb (GtkWidget              *widget,
                        GdkEventButton         *event,
                        MateNotificationApplet *applet)
{
  switch (event->button) {
    case 1: /* Left click */
      if (applet->history_context) {
        show_notification_history (applet->history_context);
      }
      return TRUE;

    case 2: /* Middle click */
      toggle_do_not_disturb (applet);
      return TRUE;

    case 3: /* Right click handled by context menu */
    default:
      break;
  }

  return FALSE;
}

static void
toggle_do_not_disturb (MateNotificationApplet *applet)
{
  gboolean current_state;

  current_state = g_settings_get_boolean (applet->settings, GSETTINGS_KEY_DO_NOT_DISTURB);
  g_settings_set_boolean (applet->settings, GSETTINGS_KEY_DO_NOT_DISTURB, !current_state);
}

static void
update_count_badge (MateNotificationApplet *applet)
{
  gchar *count_text;
  gboolean history_enabled = g_settings_get_boolean (applet->settings, GSETTINGS_KEY_HISTORY_ENABLED);

  /* Only show count badges when history is enabled and there are notifications */
  if (history_enabled && applet->unread_count > 0) {
    if (applet->unread_count > 99) {
      count_text = g_strdup ("99+");
    } else {
      count_text = g_strdup_printf ("%u", applet->unread_count);
    }
    gtk_label_set_text (GTK_LABEL (applet->count_label), count_text);
    gtk_widget_show (applet->count_label);
    g_free (count_text);
  } else {
    /* Hide badge when no unread notifications */
    gtk_widget_hide (applet->count_label);
  }
}

static gboolean
periodic_update_count (MateNotificationApplet *applet)
{
  if (applet && dbus_context_is_available (applet->dbus_context)) {
    update_unread_count (applet);
  }
  return TRUE; /* Continue periodic updates */
}

static MateNotificationApplet*
applet_main (MatePanelApplet *applet_widget)
{
  MateNotificationApplet *applet;

#ifndef ENABLE_IN_PROCESS
  g_set_application_name (_("Notification Status"));
#endif
  gtk_window_set_default_icon_name ("mate-notification-properties");

  applet = g_new (MateNotificationApplet, 1);
  applet->applet = applet_widget;
  applet->settings = g_settings_new (GSETTINGS_SCHEMA);

  /* Initialize D-Bus context and notification state */
  applet->dbus_context = dbus_context_new ();
  applet->unread_count = 0;
  applet->update_timer_id = 0;

#ifndef ENABLE_IN_PROCESS
  /* needed to clamp ourselves to the panel size */
  mate_panel_applet_set_flags (MATE_PANEL_APPLET (applet->applet), MATE_PANEL_APPLET_EXPAND_MINOR);
#endif

  /* Create overlay for icons and count badge */
  applet->overlay = gtk_overlay_new ();

  /* Create status icon */
  applet->status_image = gtk_image_new ();

  /* Add status icon as main overlay child */
  gtk_container_add (GTK_CONTAINER (applet->overlay), applet->status_image);

  /* Create count badge label */
  applet->count_label = gtk_label_new ("");
  gtk_widget_set_name (applet->count_label, "notification-count-badge");
  gtk_widget_set_halign (applet->count_label, GTK_ALIGN_END);
  gtk_widget_set_valign (applet->count_label, GTK_ALIGN_END);
  GtkCssProvider *css_provider = gtk_css_provider_new ();
  const gchar *css_data =
    "#notification-count-badge {"
    "  background-color: rgba(255,255,255,0.9);"
    "  color: #000000;"
    "  border-radius: 8px;"
    "  min-width: 12px;"
    "  min-height: 4px;"
    "  padding: 1px 1px;"
    "  font-size: 10px;"
    "  font-weight: bold;"
    "  text-shadow: none;"
    "  border: 1px solid rgba(0,0,0,0.1);"
    "}";
  gtk_css_provider_load_from_data (css_provider, css_data, -1, NULL);
  gtk_style_context_add_provider (gtk_widget_get_style_context (applet->count_label),
                                  GTK_STYLE_PROVIDER (css_provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (css_provider);

  /* Add count label as overlay */
  gtk_overlay_add_overlay (GTK_OVERLAY (applet->overlay), applet->count_label);

  /* Add overlay to applet */
  gtk_container_add (GTK_CONTAINER (applet_widget), applet->overlay);

  set_status_image (applet,
                    g_settings_get_boolean (applet->settings, GSETTINGS_KEY_DO_NOT_DISTURB),
                    g_settings_get_boolean (applet->settings, GSETTINGS_KEY_HISTORY_ENABLED));

  /* click handling */
  gtk_widget_add_events (GTK_WIDGET (applet_widget), GDK_BUTTON_PRESS_MASK);
  g_signal_connect (G_OBJECT (applet_widget), "button-press-event",
                    G_CALLBACK (applet_button_press_cb), applet);

  /* set up context menu */
  applet->action_group = gtk_action_group_new ("Notification Status Actions");
#ifdef ENABLE_NLS
  gtk_action_group_set_translation_domain (applet->action_group, GETTEXT_PACKAGE);
#endif /* ENABLE_NLS */
  gtk_action_group_add_actions (applet->action_group, applet_menu_actions,
                                G_N_ELEMENTS (applet_menu_actions), applet);

  GtkToggleAction *do_not_disturb_toggle_action =
    gtk_toggle_action_new ("DoNotDisturb", _("_Do not disturb"),
                           _("Enable/Disable do-not-disturb mode."), NULL);

  gtk_action_group_add_action (applet->action_group,
                               GTK_ACTION (do_not_disturb_toggle_action));

  GtkToggleAction *history_toggle_action =
    gtk_toggle_action_new ("HistoryEnabled", _("_Enable History"),
                           _("Enable/Disable notification history."), NULL);

  gtk_action_group_add_action (applet->action_group,
                               GTK_ACTION (history_toggle_action));

  mate_panel_applet_setup_menu_from_resource  (applet->applet,
                                               RESOURCE_PATH "menu.xml",
                                               applet->action_group);

  g_settings_bind (applet->settings, "do-not-disturb",
                   do_not_disturb_toggle_action, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (applet->settings, "history-enabled",
                   history_toggle_action, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_signal_connect (G_OBJECT (applet->applet), "destroy",
                    G_CALLBACK (applet_destroy), applet);

  /* GSettings callbacks */
  g_signal_connect (G_OBJECT (applet->settings), "changed::" GSETTINGS_KEY_DO_NOT_DISTURB,
                    G_CALLBACK (settings_changed), applet);
  g_signal_connect (G_OBJECT (applet->settings), "changed::" GSETTINGS_KEY_HISTORY_ENABLED,
                    G_CALLBACK (settings_changed), applet);

  /* Create new history context */
  applet->history_context = history_context_new (applet->dbus_context,
                                                 GTK_WIDGET (applet->applet),
                                                 G_CALLBACK (update_unread_count),
                                                 applet,
                                                 applet->settings);

  setup_daemon_connection (applet);

  /* Set up periodic updates every few seconds */
#define NOTIFICATION_UPDATE_COUNT 5
  applet->update_timer_id = g_timeout_add_seconds (NOTIFICATION_UPDATE_COUNT, (GSourceFunc) periodic_update_count, applet);

  return applet;
}

static gboolean
applet_factory (MatePanelApplet *applet_widget,
                const gchar     *iid,
                gpointer         data)
{
  MateNotificationApplet *applet;
  (void) data;

  if (!strcmp (iid, "MateNotificationApplet")) {
    applet = applet_main (applet_widget);

    g_signal_connect (G_OBJECT (applet_widget), "change_size",
                      G_CALLBACK (applet_size_changed),
                      (gpointer) applet);

    return TRUE;
  }

  return FALSE;
}

PANEL_APPLET_FACTORY ("MateNotificationAppletFactory",
                                       PANEL_TYPE_APPLET,
                                       "Notification Status Applet",
                                       applet_factory,
                                       NULL)
