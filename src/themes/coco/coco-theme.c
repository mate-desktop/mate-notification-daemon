/*
 * coco-theme.c
 * This file is part of notification-daemon-engine-coco
 *
 * Copyright (C) 2012 - Stefano Karapetsas <stefano@karapetsas.com>
 * Copyright (C) 2010 - Eduardo Grajeda
 * Copyright (C) 2008 - Martin Sourada
 *
 * notification-daemon-engine-coco is free software; you can redistribute it 
 * and/or modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 3 of the License, 
 * or (at your option) any later version.
 *
 * notification-daemon-engine-coco is distributed in the hope that it will be 
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with notification-daemon-engine-coco; if not, write to the Free 
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, 
 * Boston, MA  02110-1301  USA
 */

#include "config.h"

#include <gtk/gtk.h>

/* Define basic coco types */
typedef void (*ActionInvokedCb)(GtkWindow *nw, const char *key);
typedef void (*UrlClickedCb)(GtkWindow *nw, const char *url);

typedef struct
{
	GtkWidget *win;
	GtkWidget *top_spacer;
	GtkWidget *bottom_spacer;
	GtkWidget *main_hbox;
	GtkWidget *iconbox;
	GtkWidget *icon;
	GtkWidget *summary_label;
	GtkWidget *body_label;
	GtkWidget *actions_box;
	GtkWidget *last_sep;
	GtkWidget *stripe_spacer;
	GtkWidget *pie_countdown;

	gboolean enable_transparency;
	
	int width;
	int height;

	guchar urgency;
	glong timeout;
	glong remaining;

	UrlClickedCb url_clicked;

	GtkTextDirection rtl;
} WindowData;


enum
{
	URGENCY_LOW,
	URGENCY_NORMAL,
	URGENCY_CRITICAL
};

#define M_PI 3.14159265358979323846
#define STRIPE_WIDTH  32
#define WIDTH         300
#define IMAGE_SIZE    32
#define IMAGE_PADDING 10
#define SPACER_LEFT   30
#define PIE_RADIUS    12
#define PIE_WIDTH     (2 * PIE_RADIUS)
#define PIE_HEIGHT    (2 * PIE_RADIUS)
#define BODY_X_OFFSET (IMAGE_SIZE + 8)
#define DEFAULT_ARROW_OFFSET  (SPACER_LEFT + 12)
#define DEFAULT_ARROW_HEIGHT  14
#define DEFAULT_ARROW_WIDTH   22
#define DEFAULT_ARROW_SKEW    -6
#define BACKGROUND_OPACITY    0.90
#define GRADIENT_CENTER 0.7

/* Support Nodoka Functions */

/* Handle clicking on link */
static gboolean
activate_link (GtkLabel *label, const char *url, WindowData *windata)
{
	windata->url_clicked (windata->win, url);
	return TRUE;
}

static void
destroy_windata(WindowData *windata)
{
	g_free(windata);
}

/* Draw fuctions */
/* Standard rounded rectangle */
static void
nodoka_rounded_rectangle (cairo_t * cr,
							  double x, double y, double w, double h,
							  int radius)
{
	cairo_move_to (cr, x + radius, y);
	cairo_arc (cr, x + w - radius, y + radius, radius, M_PI * 1.5, M_PI * 2);
	cairo_arc (cr, x + w - radius, y + h - radius, radius, 0, M_PI * 0.5);
	cairo_arc (cr, x + radius, y + h - radius, radius, M_PI * 0.5, M_PI);
	cairo_arc (cr, x + radius, y + radius, radius, M_PI, M_PI * 1.5);
}

/* Fill background */
static void
fill_background(GtkWidget *widget, WindowData *windata, cairo_t *cr)
{
	float alpha;
	if (windata->enable_transparency)
		alpha = BACKGROUND_OPACITY;
	else
		alpha = 1.0;

	cairo_pattern_t *pattern;
	pattern = cairo_pattern_create_linear (0, 0, 0, windata->height);
	cairo_pattern_add_color_stop_rgba (pattern, 0, 
        19/255.0, 19/255.0, 19/255.0, alpha);
	cairo_pattern_add_color_stop_rgba (pattern, GRADIENT_CENTER, 
        19/255.0, 19/255.0, 19/255.0, alpha);
	cairo_pattern_add_color_stop_rgba (pattern, 1, 
        19/255.0, 19/255.0, 19/255.0, alpha);
	cairo_set_source (cr, pattern);
	cairo_pattern_destroy (pattern);
	
    nodoka_rounded_rectangle (cr, 0, 8, windata->width-8,
        windata->height-8, 6);

	cairo_fill (cr);	
}

static void
draw_pie(GtkWidget *pie, WindowData *windata, cairo_t *cr)
{
	gdouble arc_angle = 1.0 - (gdouble)windata->remaining / (gdouble)windata->timeout;
	cairo_set_source_rgba (cr, 1.0, 0.4, 0.0, 0.3);
	cairo_move_to(cr, PIE_RADIUS, PIE_RADIUS);
	cairo_arc_negative(cr, PIE_RADIUS, PIE_RADIUS, PIE_RADIUS,
					-M_PI/2, (-0.25 + arc_angle)*2*M_PI);
	cairo_line_to(cr, PIE_RADIUS, PIE_RADIUS);
	
	cairo_fill (cr); 
}

static gboolean
paint_window(GtkWidget *widget,
#if GTK_CHECK_VERSION (3, 0, 0)
			 cairo_t *cr,
#else
			 GdkEventExpose *event,
#endif
			 WindowData *windata)
{
	cairo_surface_t *surface;
	cairo_t *context;
	GtkAllocation allocation;
#if !GTK_CHECK_VERSION (3, 0, 0)
	cairo_t *cr;
#endif

	if (windata->width == 0) {
#if GTK_CHECK_VERSION(3, 0, 0)
		gtk_widget_get_allocation(windata->win, &allocation);
#else
		allocation = windata->win->allocation;
#endif
		windata->width = allocation.width;
		windata->height = allocation.height;    
	}
	
	if (!(windata->enable_transparency))
	{
#if GTK_CHECK_VERSION (3, 0, 0)
			surface = cairo_surface_create_similar (cairo_get_target (cr),
													CAIRO_CONTENT_COLOR_ALPHA,
													windata->width,
													windata->height);

			cairo_save (cr);
			cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
			cairo_set_source_surface (cr, surface, 0, 0);
			cairo_paint (cr);

			cairo_set_source_rgba (cr, 1, 1, 1, 1);
            nodoka_rounded_rectangle (cr, 0, 0, 
                windata->width, windata->height, 6);
			cairo_fill (cr);

			cairo_restore (cr);
			cairo_surface_destroy(surface);
#else
			GdkPixmap *mask;
			cairo_t *mask_cr;
			mask = gdk_pixmap_new (NULL, windata->width, 
						     windata->height, 1);
			mask_cr = gdk_cairo_create ((GdkDrawable *) mask);
			cairo_set_operator (mask_cr, CAIRO_OPERATOR_CLEAR);
			cairo_paint (mask_cr);

			cairo_set_operator (mask_cr, CAIRO_OPERATOR_OVER);
			cairo_set_source_rgba (mask_cr, 1, 1, 1, 1);
            nodoka_rounded_rectangle (mask_cr, 0, 0, 
                windata->width, windata->height, 6);
			cairo_fill (mask_cr);
			gdk_window_shape_combine_mask (windata->win->window,
						       (GdkBitmap *) mask, 0,0);
			gdk_pixmap_unref (mask);
			cairo_destroy (mask_cr);
#endif
	}

#if GTK_CHECK_VERSION(3, 0, 0)
	context = gdk_cairo_create(gtk_widget_get_window(widget));
#else 
	context = gdk_cairo_create(widget->window);
#endif

	cairo_set_operator(context, CAIRO_OPERATOR_SOURCE);

#if GTK_CHECK_VERSION (3, 0, 0)
	surface = cairo_surface_create_similar(cairo_get_target(context),
										   CAIRO_CONTENT_COLOR_ALPHA,
										   windata->width,
										   windata->height);
	cairo_set_source_surface (cr, surface, 0, 0);
#else
	surface = cairo_surface_create_similar(cairo_get_target(context),
										   CAIRO_CONTENT_COLOR_ALPHA,
										   widget->allocation.width,
										   widget->allocation.height);
	cr = cairo_create(surface);
#endif

	fill_background(widget, windata, cr);

#if !GTK_CHECK_VERSION (3, 0, 0)
	cairo_destroy(cr);
#endif
	cairo_set_source_surface(context, surface, 0, 0);
	cairo_paint(context);
	cairo_surface_destroy(surface);
	cairo_destroy(context);

	return FALSE;
}

/* Event handlers */
static gboolean
configure_event_cb(GtkWidget *nw,
				   GdkEventConfigure *event,
				   WindowData *windata)
{
	windata->width = event->width;
	windata->height = event->height;

	gtk_widget_queue_draw(nw);

	return FALSE;
}

static gboolean
countdown_expose_cb(GtkWidget *pie,
#if GTK_CHECK_VERSION (3, 0, 0)
					cairo_t *cr,
#else
					GdkEventExpose *event,
#endif
					WindowData *windata)
{
	cairo_t *context;
	cairo_surface_t *surface;
#if GTK_CHECK_VERSION (3, 0, 0)
	GtkAllocation alloc; 
#else
	cairo_t *cr;
#endif

#if GTK_CHECK_VERSION(3, 0, 0)
	context = gdk_cairo_create(gtk_widget_get_window(pie));
#else 
	context = gdk_cairo_create(pie->window);
#endif

	cairo_set_operator(context, CAIRO_OPERATOR_SOURCE);
#if GTK_CHECK_VERSION (3, 0, 0)
	gtk_widget_get_allocation(pie, &alloc);
	surface = cairo_surface_create_similar(cairo_get_target(context),
										   CAIRO_CONTENT_COLOR_ALPHA,
										   alloc.width,
										   alloc.height); 
#else
	surface = cairo_surface_create_similar(cairo_get_target(context),
										   CAIRO_CONTENT_COLOR_ALPHA,
										   pie->allocation.width,
										   pie->allocation.height);
#endif
	cr = cairo_create(surface);

#if GTK_CHECK_VERSION(3, 0, 0)
	cairo_translate (cr, -alloc.x, -alloc.y);
	fill_background (pie, windata, cr);
	cairo_translate (cr, alloc.x, alloc.y);
#else 
	cairo_translate (cr, -pie->allocation.x, -pie->allocation.y);
	fill_background (pie, windata, cr);
	cairo_translate (cr, pie->allocation.x, pie->allocation.y);
#endif
	draw_pie (pie, windata, cr);

#if !GTK_CHECK_VERSION(3, 0, 0)
	cairo_destroy(cr);
#endif
	cairo_set_source_surface(context, surface, 0, 0);
	cairo_paint(context);
	cairo_surface_destroy(surface);
	cairo_destroy(context);
	return TRUE;
}

static void
action_clicked_cb(GtkWidget *w, GdkEventButton *event,
				  ActionInvokedCb action_cb)
{
	GtkWindow *nw   = g_object_get_data(G_OBJECT(w), "_nw");
	const char *key = g_object_get_data(G_OBJECT(w), "_action_key");

	action_cb(nw, key);
}



/* Required functions */

/* Checking if we support this notification daemon version */
gboolean
theme_check_init(unsigned int major_ver, unsigned int minor_ver,
				 unsigned int micro_ver)
{
	return major_ver == NOTIFICATION_DAEMON_MAJOR_VERSION && minor_ver == NOTIFICATION_DAEMON_MINOR_VERSION && micro_ver == NOTIFICATION_DAEMON_MICRO_VERSION;
}

/* Sending theme info to the notification daemon */
void
get_theme_info(char **theme_name,
			   char **theme_ver,
			   char **author,
			   char **homepage)
{
	*theme_name = g_strdup("Coco");
	*theme_ver  = g_strdup_printf("%d.%d.%d", NOTIFICATION_DAEMON_MAJOR_VERSION, 
                                                  NOTIFICATION_DAEMON_MINOR_VERSION, 
						  NOTIFICATION_DAEMON_MICRO_VERSION);
	*author = g_strdup("Eduardo Grajeda");
	*homepage = g_strdup("http://github.com/tatofoo/");
}

/* Create new notification */
GtkWindow *
create_notification(UrlClickedCb url_clicked)
{
	GtkWidget *win;
	GtkWidget *drawbox;
	GtkWidget *main_vbox;
	GtkWidget *vbox;
	GtkWidget *alignment;
    GtkWidget *padding;
	AtkObject *atkobj;
	WindowData *windata;
#if GTK_CHECK_VERSION(3, 0, 0)
	GdkVisual *visual;
#else 
	GdkColormap *colormap;
#endif
	GdkScreen *screen;

	windata = g_new0(WindowData, 1);
	windata->urgency = URGENCY_NORMAL;
	windata->url_clicked = url_clicked;

	win = gtk_window_new(GTK_WINDOW_POPUP);
	windata->win = win;

	windata->rtl = gtk_widget_get_default_direction();
	windata->enable_transparency = FALSE;
	screen = gtk_window_get_screen(GTK_WINDOW(win));
#if GTK_CHECK_VERSION(3, 0, 0)
	visual = gdk_screen_get_rgba_visual(screen);

	if (visual != NULL)
	{
		gtk_widget_set_visual(win, visual);
		if (gdk_screen_is_composited(screen))
			windata->enable_transparency = TRUE;
	}
#else 
	colormap = gdk_screen_get_rgba_colormap(screen);

	if (colormap != NULL)
	{
		gtk_widget_set_colormap(win, colormap);
		if (gdk_screen_is_composited(screen))
			windata->enable_transparency = TRUE;
	}
#endif

	gtk_window_set_title(GTK_WINDOW(win), "Notification");
	gtk_widget_add_events(win, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
	gtk_widget_realize(win);

	g_object_set_data_full(G_OBJECT(win), "windata", windata,
						   (GDestroyNotify)destroy_windata);
	atk_object_set_role(gtk_widget_get_accessible(win), ATK_ROLE_ALERT);

	g_signal_connect(G_OBJECT(win), "configure_event",
					 G_CALLBACK(configure_event_cb), windata);

	/*
	 * For some reason, there are occasionally graphics glitches when
	 * repainting the window. Despite filling the window with a background
	 * color, parts of the other windows on the screen or the shadows around
	 * notifications will appear on the notification. Somehow, adding this
	 * eventbox makes that problem just go away. Whatever works for now.
	 */
	drawbox = gtk_event_box_new();
	gtk_widget_show(drawbox);
	gtk_container_add(GTK_CONTAINER(win), drawbox);

	main_vbox = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(main_vbox);
	gtk_container_add(GTK_CONTAINER(drawbox), main_vbox);

#if GTK_CHECK_VERSION (3, 0, 0)
	g_signal_connect(G_OBJECT(main_vbox), "draw",
					 G_CALLBACK(paint_window), windata);
#else
	g_signal_connect(G_OBJECT(main_vbox), "expose_event",
					 G_CALLBACK(paint_window), windata);
#endif

    padding = gtk_alignment_new(0, 0, 0, 0);
	gtk_widget_show(padding);
	gtk_box_pack_start(GTK_BOX(main_vbox), padding, FALSE, FALSE, 0);
  g_object_set(G_OBJECT(padding), "top-padding", 8, "right-padding", 8, NULL);

	windata->main_hbox = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(windata->main_hbox);
	gtk_container_add(GTK_CONTAINER(padding), windata->main_hbox);
	gtk_container_set_border_width(GTK_CONTAINER(windata->main_hbox), 13);
   
    /* The icon goes at the left */ 
	windata->iconbox = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(windata->iconbox);
	gtk_box_pack_start(GTK_BOX(windata->main_hbox), windata->iconbox,
					   FALSE, FALSE, 0);
    
	windata->icon = gtk_image_new();
	gtk_box_pack_start(GTK_BOX(windata->iconbox), windata->icon,
					   FALSE, FALSE, 0);

    /* The title and the text at the right */
    padding = gtk_alignment_new(0, 0.5, 0, 0);
	gtk_widget_show(padding);
	gtk_box_pack_start(GTK_BOX(windata->main_hbox), padding, TRUE, TRUE, 0);
  g_object_set(G_OBJECT(padding), "left-padding", 8, NULL);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(vbox);
	gtk_container_add(GTK_CONTAINER(padding), vbox);

	windata->summary_label = gtk_label_new(NULL);
	gtk_widget_show(windata->summary_label);
	gtk_box_pack_start(GTK_BOX(vbox), windata->summary_label, FALSE, FALSE, 0);
	gtk_misc_set_alignment(GTK_MISC(windata->summary_label), 0, 0);
	gtk_label_set_line_wrap(GTK_LABEL(windata->summary_label), TRUE);

	atkobj = gtk_widget_get_accessible(windata->summary_label);
	atk_object_set_description(atkobj, "Notification summary text.");

	windata->body_label = gtk_label_new(NULL);
	gtk_box_pack_start(GTK_BOX(vbox), windata->body_label, FALSE, FALSE, 0);
	gtk_misc_set_alignment(GTK_MISC(windata->body_label), 0, 0);
	gtk_label_set_line_wrap(GTK_LABEL(windata->body_label), TRUE);
	g_signal_connect(G_OBJECT(windata->body_label), "activate-link",
                         G_CALLBACK(activate_link), windata);

	atkobj = gtk_widget_get_accessible(windata->body_label);
	atk_object_set_description(atkobj, "Notification body text.");

    /* Disabled for now */
	alignment = gtk_alignment_new(1, 0.5, 0, 0);
	gtk_widget_show(alignment);
    gtk_widget_hide(alignment);
	gtk_box_pack_start(GTK_BOX(vbox), alignment, FALSE, TRUE, 0);

	windata->actions_box = gtk_hbox_new(FALSE, 6);
	gtk_container_add(GTK_CONTAINER(alignment), windata->actions_box);

	return GTK_WINDOW(win);
}

/* Set the notification text */
void
set_notification_text(GtkWindow *nw, const char *summary, const char *body)
{
	char *str;
	WindowData *windata = g_object_get_data(G_OBJECT(nw), "windata");
	g_assert(windata != NULL);

	str = g_strdup_printf(
        "<span color=\"#FFFFFF\"><big><b>%s</b></big></span>", summary);
	gtk_label_set_markup(GTK_LABEL(windata->summary_label), str);
	g_free(str);

	str = g_strdup_printf(
        "<span color=\"#EAEAEA\">%s</span>", body);
	gtk_label_set_markup (GTK_LABEL (windata->body_label), str);
	g_free(str);

	if (body == NULL || *body == '\0')
		gtk_widget_hide(windata->body_label);
	else
		gtk_widget_show(windata->body_label);

	gtk_widget_set_size_request(
		((body != NULL && *body != '\0')
		 ? windata->body_label : windata->summary_label),
		WIDTH - (IMAGE_SIZE + IMAGE_PADDING) - 10,
		-1);
}

/* Set notification icon */
void
set_notification_icon(GtkWindow *nw, GdkPixbuf *pixbuf)
{
	WindowData *windata = g_object_get_data(G_OBJECT(nw), "windata");
	g_assert(windata != NULL);

	gtk_image_set_from_pixbuf(GTK_IMAGE(windata->icon), pixbuf);

	if (pixbuf != NULL)
	{
		int pixbuf_width = gdk_pixbuf_get_width(pixbuf);

		gtk_widget_show(windata->icon);
		gtk_widget_set_size_request(windata->iconbox,
									MAX(BODY_X_OFFSET, pixbuf_width), -1);
	}
	else
	{
		gtk_widget_hide(windata->icon);
		gtk_widget_set_size_request(windata->iconbox, BODY_X_OFFSET, -1);
	}
}

/* Set notification arrow */
void
set_notification_arrow(GtkWidget *nw, gboolean visible, int x, int y)
{
    /* nothing */
}

/* Add notification action */
void
add_notification_action(GtkWindow *nw, const char *text, const char *key,
						ActionInvokedCb cb)
{
	WindowData *windata = g_object_get_data(G_OBJECT(nw), "windata");
	GtkWidget *label;
	GtkWidget *button;
	GtkWidget *hbox;
	GdkPixbuf *pixbuf;
	char *buf;

	g_assert(windata != NULL);

#if GTK_CHECK_VERSION(3, 0, 0)
	if (gtk_widget_get_visible(windata->actions_box))
#else 
	if (!GTK_WIDGET_VISIBLE(windata->actions_box))
#endif
	{
		GtkWidget *alignment;

		gtk_widget_show(windata->actions_box);

		alignment = gtk_alignment_new(1, 0.5, 0, 0);
		gtk_widget_show(alignment);
		gtk_box_pack_end(GTK_BOX(windata->actions_box), alignment,
						   FALSE, TRUE, 0);

		windata->pie_countdown = gtk_drawing_area_new();
		gtk_widget_show(windata->pie_countdown);
		gtk_container_add(GTK_CONTAINER(alignment), windata->pie_countdown);
		gtk_widget_set_size_request(windata->pie_countdown,
									PIE_WIDTH, PIE_HEIGHT);
#if GTK_CHECK_VERSION (3, 0, 0)
		g_signal_connect(G_OBJECT(windata->pie_countdown), "draw",
						 G_CALLBACK(countdown_expose_cb), windata);
#else
		g_signal_connect(G_OBJECT(windata->pie_countdown), "expose_event",
						 G_CALLBACK(countdown_expose_cb), windata);
#endif
	}

	button = gtk_button_new();
	gtk_widget_show(button);
	gtk_box_pack_start(GTK_BOX(windata->actions_box), button, FALSE, FALSE, 0);

	hbox = gtk_hbox_new(FALSE, 6);
	gtk_widget_show(hbox);
	gtk_container_add(GTK_CONTAINER(button), hbox);

	/* Try to be smart and find a suitable icon. */
	buf = g_strdup_printf("stock_%s", key);
	pixbuf = gtk_icon_theme_load_icon(
		gtk_icon_theme_get_for_screen(
#if GTK_CHECK_VERSION(3, 0, 0)
			gdk_window_get_screen(gtk_widget_get_window(GTK_WIDGET(nw)))),
#else 
			gdk_drawable_get_screen(GTK_WIDGET(nw)->window)),
#endif
		buf, 16, GTK_ICON_LOOKUP_USE_BUILTIN, NULL);
	g_free(buf);

	if (pixbuf != NULL)
	{
		GtkWidget *image = gtk_image_new_from_pixbuf(pixbuf);
		gtk_widget_show(image);
		gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);
		gtk_misc_set_alignment(GTK_MISC(image), 0.5, 0.5);
	}

	label = gtk_label_new(NULL);
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	buf = g_strdup_printf("<small>%s</small>", text);
	gtk_label_set_markup(GTK_LABEL(label), buf);
	g_free(buf);

	g_object_set_data(G_OBJECT(button), "_nw", nw);
	g_object_set_data_full(G_OBJECT(button),
						   "_action_key", g_strdup(key), g_free);
	g_signal_connect(G_OBJECT(button), "button-release-event",
					 G_CALLBACK(action_clicked_cb), cb);
}

/* Clear notification actions */
void
clear_notification_actions(GtkWindow *nw)
{
	WindowData *windata = g_object_get_data(G_OBJECT(nw), "windata");

	windata->pie_countdown = NULL;

	gtk_widget_hide(windata->actions_box);
	gtk_container_foreach(GTK_CONTAINER(windata->actions_box),
#if GTK_CHECK_VERSION (3, 0, 0)
						  (GtkCallback)g_object_unref, NULL);
#else
						  (GtkCallback)gtk_object_destroy, NULL);
#endif
}

/* Move notification window */
void
move_notification(GtkWidget *nw, int x, int y)
{
	WindowData *windata = g_object_get_data(G_OBJECT(nw), "windata");
	g_assert(windata != NULL);

    gtk_window_move(GTK_WINDOW(nw), x, y);
}


/* Optional Functions */

/* Destroy notification */

/* Show notification */

/* Hide notification */

/* Set notification timeout */
void
set_notification_timeout(GtkWindow *nw, glong timeout)
{
	WindowData *windata = g_object_get_data(G_OBJECT(nw), "windata");
	g_assert(windata != NULL);

	windata->timeout = timeout;
}

/* Set notification hints */
void
set_notification_hints(GtkWindow *nw, GHashTable *hints)
{
	WindowData *windata = g_object_get_data(G_OBJECT(nw), "windata");
	GValue *value;

	g_assert(windata != NULL);

	value = (GValue *)g_hash_table_lookup(hints, "urgency");

	if (value != NULL && G_VALUE_HOLDS_UCHAR(value))
	{
		windata->urgency = g_value_get_uchar(value);

		if (windata->urgency == URGENCY_CRITICAL) {
			gtk_window_set_title(GTK_WINDOW(nw), "Critical Notification");
		} else {
			gtk_window_set_title(GTK_WINDOW(nw), "Notification");
		}
	}
}

/* Notification tick */
void
notification_tick(GtkWindow *nw, glong remaining)
{
	WindowData *windata = g_object_get_data(G_OBJECT(nw), "windata");
	windata->remaining = remaining;

	if (windata->pie_countdown != NULL)
	{
		gtk_widget_queue_draw_area(windata->pie_countdown, 0, 0,
								   PIE_WIDTH, PIE_HEIGHT);
	}
}
