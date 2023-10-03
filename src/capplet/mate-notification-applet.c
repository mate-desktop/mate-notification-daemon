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
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <mate-panel-applet.h>

#define MATE_DESKTOP_USE_UNSTABLE_API
#include <libmate-desktop/mate-desktop-utils.h>

#include "constants.h"

typedef struct
{
  MatePanelApplet *applet;

  GtkWidget       *image_on;
  GtkWidget       *image_off;
  GtkActionGroup  *action_group;
  GSettings       *settings;
} MateNotificationApplet;

static void
show_about      (GtkAction              *action,
                 MateNotificationApplet *applet);
static void
call_properties (GtkAction              *action,
                 MateNotificationApplet *applet);

static const GtkActionEntry applet_menu_actions [] = {
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

  g_object_unref (applet->settings);
  g_object_unref (applet->action_group);
  g_free (applet);
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
    "Robert Buj <robert.buj@gmail.com>",
    NULL
  };
  (void) action;
  (void) applet;

  gtk_show_about_dialog (NULL,
                         "title", _("About Do Not Disturb"),
                         "version", VERSION,
                         "copyright", _("Copyright \xc2\xa9 2021 MATE developers"),
                         "comments", _("Activate the do not disturb mode quickly."),
                         "authors", authors,
                         "translator-credits", _("translator-credits"),
                         "logo_icon_name", "mate-notification-properties",
                         NULL);
}

static void
set_status_image (MateNotificationApplet *applet,
                  gboolean                active)
{
  gtk_widget_show_all (GTK_WIDGET (applet->applet));
  if (active)
    gtk_widget_hide (applet->image_on);
  else
    gtk_widget_hide (applet->image_off);
}

static void
settings_changed (GSettings              *settings,
                  gchar                  *key,
                  MateNotificationApplet *applet)
{
  if (g_strcmp0 (GSETTINGS_KEY_DO_NOT_DISTURB, key) == 0)
    set_status_image (applet,
                      g_settings_get_boolean (settings, key));
}

static void
applet_draw_icon (MatePanelApplet *applet_widget,
                  int arg1,
                  MateNotificationApplet *applet)
{
  gint size, scale;

  g_assert (applet);

  size = (gint) mate_panel_applet_get_size (applet_widget);
  scale = gtk_widget_get_scale_factor (GTK_WIDGET (applet_widget));

  cairo_surface_t *image_on = gtk_icon_theme_load_surface (gtk_icon_theme_get_default (),
                                                           "user-available",
                                                           size, scale,
                                                           NULL, 0, NULL);
  cairo_surface_t *image_off = gtk_icon_theme_load_surface (gtk_icon_theme_get_default (),
                                                            "user-invisible",
                                                            size, scale,
                                                            NULL, 0, NULL);

  gtk_image_set_from_surface (GTK_IMAGE (applet->image_on), image_on);
  gtk_image_set_from_surface (GTK_IMAGE (applet->image_off), image_off);

  cairo_surface_destroy (image_on);
  cairo_surface_destroy (image_off);
}

static MateNotificationApplet*
applet_main (MatePanelApplet *applet_widget)
{
  MateNotificationApplet *applet;
  GtkWidget *box;

#ifdef ENABLE_NLS
  bindtextdomain (GETTEXT_PACKAGE, MATELOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);
#endif /* ENABLE_NLS */

#ifndef ENABLE_IN_PROCESS
  g_set_application_name (_("Do Not Disturb"));
#endif
  gtk_window_set_default_icon_name ("mate-notification-properties");

  applet = g_new (MateNotificationApplet, 1);
  applet->applet = applet_widget;
  applet->settings = g_settings_new (GSETTINGS_SCHEMA);

#ifndef ENABLE_IN_PROCESS
  /* needed to clamp ourselves to the panel size */
  mate_panel_applet_set_flags (MATE_PANEL_APPLET (applet), MATE_PANEL_APPLET_EXPAND_MINOR);
#endif
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  applet->image_on  = gtk_image_new ();
  applet->image_off = gtk_image_new ();

  applet_draw_icon (applet_widget, 0, applet);

  gtk_widget_set_tooltip_text (applet->image_off, _("Do Not Disturb"));
  gtk_widget_set_tooltip_text (applet->image_on, _("Notifications Enabled"));

  gtk_box_pack_start (GTK_BOX (box), applet->image_on,
                      TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), applet->image_off,
                      TRUE, TRUE, 0);
  gtk_container_add (GTK_CONTAINER (applet_widget), box);

  set_status_image (applet,
                    g_settings_get_boolean (applet->settings,
                                            GSETTINGS_KEY_DO_NOT_DISTURB));

  /* set up context menu */
  applet->action_group = gtk_action_group_new ("Do Not Disturb Actions");
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

  mate_panel_applet_setup_menu_from_resource  (applet->applet,
                                               RESOURCE_PATH "menu.xml",
                                               applet->action_group);

  g_settings_bind (applet->settings, "do-not-disturb",
                   do_not_disturb_toggle_action, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_signal_connect (G_OBJECT (applet->applet), "destroy",
                    G_CALLBACK (applet_destroy), applet);

  /* GSettings callback */
  g_signal_connect (G_OBJECT (applet->settings), "changed::" GSETTINGS_KEY_DO_NOT_DISTURB,
                    G_CALLBACK (settings_changed), applet);

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
                      G_CALLBACK (applet_draw_icon),
                      (gpointer) applet);

    return TRUE;
  }

  return FALSE;
}

PANEL_APPLET_FACTORY ("MateNotificationAppletFactory",
                                       PANEL_TYPE_APPLET,
                                       "Do Not Disturb Applet",
                                       applet_factory,
                                       NULL)
