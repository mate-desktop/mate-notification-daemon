/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2006-2007 Christian Hammond <chipx86@chipx86.com>
 * Copyright (C) 2009 Red Hat, Inc.
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
#include <gtk/gtk.h>

#include <libxml/xpath.h>

typedef void (*ActionInvokedCb) (GtkWindow* nw, const char* key);
typedef void (*UrlClickedCb) (GtkWindow* nw, const char* url);

typedef struct {
	GtkWidget* win;
	GtkWidget* top_spacer;
	GtkWidget* bottom_spacer;
	GtkWidget* main_hbox;
	GtkWidget* iconbox;
	GtkWidget* icon;
	GtkWidget* content_hbox;
	GtkWidget* summary_label;
	GtkWidget* close_button;
	GtkWidget* body_label;
	GtkWidget* actions_box;
	GtkWidget* last_sep;
	GtkWidget* stripe_spacer;
	GtkWidget* pie_countdown;

	gboolean has_arrow;
	gboolean composited;
	gboolean action_icons;

	int point_x;
	int point_y;

	int drawn_arrow_begin_x;
	int drawn_arrow_begin_y;
	int drawn_arrow_middle_x;
	int drawn_arrow_middle_y;
	int drawn_arrow_end_x;
	int drawn_arrow_end_y;

	int width;
	int height;

	GdkPoint* border_points;
	size_t num_border_points;

	cairo_region_t *window_region;

	guchar urgency;
	glong timeout;
	glong remaining;

	UrlClickedCb url_clicked;

} WindowData;

enum {
	URGENCY_LOW,
	URGENCY_NORMAL,
	URGENCY_CRITICAL
};

gboolean theme_check_init(unsigned int major_ver, unsigned int minor_ver,
			  unsigned int micro_ver);
void get_theme_info(char **theme_name, char **theme_ver, char **author,
		    char **homepage);
GtkWindow* create_notification(UrlClickedCb url_clicked);
void set_notification_text(GtkWindow *nw, const char *summary,
			   const char *body);
void set_notification_icon(GtkWindow *nw, GdkPixbuf *pixbuf);
void set_notification_arrow(GtkWidget *nw, gboolean visible, int x, int y);
void add_notification_action(GtkWindow *nw, const char *text, const char *key,
			     ActionInvokedCb cb);
void clear_notification_actions(GtkWindow *nw);
void move_notification(GtkWidget *nw, int x, int y);
void set_notification_timeout(GtkWindow *nw, glong timeout);
void set_notification_hints(GtkWindow *nw, GVariant *hints);
void notification_tick(GtkWindow *nw, glong remaining);

//#define ENABLE_GRADIENT_LOOK

#ifdef ENABLE_GRADIENT_LOOK
#define STRIPE_WIDTH 45
#else
#define STRIPE_WIDTH 30
#endif

#define WIDTH         400
#define IMAGE_SIZE    32
#define IMAGE_PADDING 10
#define SPACER_LEFT   30
#define PIE_RADIUS    12
#define PIE_WIDTH     (2 * PIE_RADIUS)
#define PIE_HEIGHT    (2 * PIE_RADIUS)
#define BODY_X_OFFSET (IMAGE_SIZE + 8)
#define DEFAULT_ARROW_OFFSET  (SPACER_LEFT + 2)
#define DEFAULT_ARROW_HEIGHT  14
#define DEFAULT_ARROW_WIDTH   28
#define BACKGROUND_OPACITY    0.92
#define BOTTOM_GRADIENT_HEIGHT 30

static void
get_background_color (GtkStyleContext *context,
                      GtkStateFlags    state,
                      GdkRGBA         *color)
{
        GdkRGBA *c;

        g_return_if_fail (color != NULL);
        g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

        gtk_style_context_get (context, state,
                               "background-color", &c,
                               NULL);

        *color = *c;
        gdk_rgba_free (c);
}

static void fill_background(GtkWidget* widget, WindowData* windata, cairo_t* cr)
{
    GtkStyleContext *context;
    GdkRGBA bg;

    GtkAllocation allocation;

    gtk_widget_get_allocation(widget, &allocation);

#ifdef ENABLE_GRADIENT_LOOK
    cairo_pattern_t *gradient;
    int              gradient_y;

    gradient_y = allocation.height - BOTTOM_GRADIENT_HEIGHT;
#endif

    context = gtk_widget_get_style_context (windata->win);

    gtk_style_context_save (context);
    gtk_style_context_set_state (context, GTK_STATE_FLAG_NORMAL);

    get_background_color (context, GTK_STATE_FLAG_NORMAL, &bg);

    gtk_style_context_restore (context);

    if (windata->composited)
    {
        cairo_set_source_rgba(cr, bg.red, bg.green, bg.blue, BACKGROUND_OPACITY);
    }
    else
    {
        gdk_cairo_set_source_rgba (cr, &bg);
    }

    cairo_rectangle(cr, 0, 0, allocation.width, allocation.height);

    cairo_fill(cr);

#ifdef ENABLE_GRADIENT_LOOK
    /* Add a very subtle gradient to the bottom of the notification */
    gradient = cairo_pattern_create_linear(0, gradient_y, 0, allocation.height);
    cairo_pattern_add_color_stop_rgba(gradient, 0, 0, 0, 0, 0);
    cairo_pattern_add_color_stop_rgba(gradient, 1, 0, 0, 0, 0.15);
    cairo_rectangle(cr, 0, gradient_y, allocation.width, BOTTOM_GRADIENT_HEIGHT);

    cairo_set_source(cr, gradient);
    cairo_fill(cr);
    cairo_pattern_destroy(gradient);
#endif
}

static void draw_stripe(GtkWidget* widget, WindowData* windata, cairo_t* cr)
{
	GtkStyleContext* context;
	GdkRGBA bg;
	int              stripe_x;
	int              stripe_y;
	int              stripe_height;
#ifdef ENABLE_GRADIENT_LOOK
	cairo_pattern_t* gradient;
	double           r, g, b;
#endif

	context = gtk_widget_get_style_context (widget);

	gtk_style_context_save (context);

	GtkAllocation alloc;
	gtk_widget_get_allocation(windata->main_hbox, &alloc);

	stripe_x = alloc.x + 1;

	if (gtk_widget_get_direction(widget) == GTK_TEXT_DIR_RTL)
	{
		stripe_x = windata->width - STRIPE_WIDTH - stripe_x;
	}

	stripe_y = alloc.y + 1;
	stripe_height = alloc.height - 2;

	switch (windata->urgency)
	{
		case URGENCY_LOW: // LOW
			gtk_style_context_set_state (context, GTK_STATE_FLAG_NORMAL);
			gtk_style_context_add_class (context, GTK_STYLE_CLASS_VIEW);
			get_background_color (context, GTK_STATE_FLAG_NORMAL, &bg);
			gdk_cairo_set_source_rgba (cr, &bg);
			break;

		case URGENCY_CRITICAL: // CRITICAL
			gdk_rgba_parse (&bg, "#CC0000");
			break;

		case URGENCY_NORMAL: // NORMAL
		default:
			gtk_style_context_set_state (context, GTK_STATE_FLAG_SELECTED);
			gtk_style_context_add_class (context, GTK_STYLE_CLASS_VIEW);
			get_background_color (context, GTK_STATE_FLAG_SELECTED, &bg);
			gdk_cairo_set_source_rgba (cr, &bg);
			break;
	}

	gtk_style_context_restore (context);

	cairo_rectangle(cr, stripe_x, stripe_y, STRIPE_WIDTH, stripe_height);

#ifdef ENABLE_GRADIENT_LOOK
	r = color.red / 65535.0;
	g = color.green / 65535.0;
	b = color.blue / 65535.0;

	gradient = cairo_pattern_create_linear(stripe_x, 0, STRIPE_WIDTH, 0);
	cairo_pattern_add_color_stop_rgba(gradient, 0, r, g, b, 1);
	cairo_pattern_add_color_stop_rgba(gradient, 1, r, g, b, 0);
	cairo_set_source(cr, gradient);
	cairo_fill(cr);
	cairo_pattern_destroy(gradient);
#else
	gdk_cairo_set_source_rgba (cr, &bg);
	cairo_fill(cr);
#endif
}

static GtkArrowType get_notification_arrow_type(GtkWidget* nw)
{
	WindowData*     windata;
	GdkScreen*      screen;
	GdkRectangle    monitor_geometry;
	GdkDisplay*     display;
	GdkMonitor*     monitor;

	windata = g_object_get_data(G_OBJECT(nw), "windata");

	screen = gdk_window_get_screen(GDK_WINDOW( gtk_widget_get_window(nw)));
	display = gdk_screen_get_display (screen);
	monitor = gdk_display_get_monitor_at_point (display, windata->point_x, windata->point_y);
	gdk_monitor_get_geometry (monitor, &monitor_geometry);

	if (windata->point_y - monitor_geometry.y + windata->height + DEFAULT_ARROW_HEIGHT > monitor_geometry.height)
	{
		return GTK_ARROW_DOWN;
	}
	else
	{
		return GTK_ARROW_UP;
	}
}

#define ADD_POINT(_x, _y, shapeoffset_x, shapeoffset_y) \
	G_STMT_START { \
		windata->border_points[i].x = (_x); \
		windata->border_points[i].y = (_y); \
		shape_points[i].x = (_x) + (shapeoffset_x); \
		shape_points[i].y = (_y) + (shapeoffset_y); \
		i++;\
	} G_STMT_END

static void create_border_with_arrow(GtkWidget* nw, WindowData* windata)
{
	int             width;
	int             height;
	int             y;
	int             norm_point_x;
	int             norm_point_y;
	GtkArrowType    arrow_type;
	GdkScreen*      screen;
	int             arrow_side1_width = DEFAULT_ARROW_WIDTH / 2;
	int             arrow_side2_width = DEFAULT_ARROW_WIDTH / 2;
	int             arrow_offset = DEFAULT_ARROW_OFFSET;
	GdkPoint*       shape_points = NULL;
	int             i = 0;
	GdkMonitor*     monitor;
	GdkDisplay*     display;
	GdkRectangle    monitor_geometry;

	width = windata->width;
	height = windata->height;

	screen = gdk_window_get_screen(GDK_WINDOW(gtk_widget_get_window(nw)));
	display = gdk_screen_get_display (screen);
	monitor = gdk_display_get_monitor_at_point (display, windata->point_x, windata->point_y);
	gdk_monitor_get_geometry (monitor, &monitor_geometry);

	windata->num_border_points = 5;

	arrow_type = get_notification_arrow_type(windata->win);

	norm_point_x = windata->point_x - monitor_geometry.x;
	norm_point_y = windata->point_y - monitor_geometry.y;

	/* Handle the offset and such */
	switch (arrow_type)
	{
		case GTK_ARROW_UP:
		case GTK_ARROW_DOWN:

			if (norm_point_x < arrow_side1_width)
			{
				arrow_side1_width = 0;
				arrow_offset = 0;
			}
			else if (norm_point_x > monitor_geometry.width - arrow_side2_width)
			{
				arrow_side2_width = 0;
				arrow_offset = width - arrow_side1_width;
			}
			else
			{
				if (norm_point_x - arrow_side2_width + width >= monitor_geometry.width)
				{
					arrow_offset = width - monitor_geometry.width + norm_point_x;
				}
				else
				{
					arrow_offset = MIN(norm_point_x - arrow_side1_width, DEFAULT_ARROW_OFFSET);
				}

				if (arrow_offset == 0 || arrow_offset == width - arrow_side1_width)
				{
					windata->num_border_points++;
				}
				else
				{
					windata->num_border_points += 2;
				}
			}

			/*
			 * Why risk this for official builds? If it's somehow off the
			 * screen, it won't horribly impact the user. Definitely less
			 * than an assertion would...
			 */
			#if 0
				g_assert(arrow_offset + arrow_side1_width >= 0);
				g_assert(arrow_offset + arrow_side1_width + arrow_side2_width <= width);
			#endif

			windata->border_points = g_new0(GdkPoint, windata->num_border_points);
			shape_points = g_new0(GdkPoint, windata->num_border_points);

			windata->drawn_arrow_begin_x = arrow_offset;
			windata->drawn_arrow_middle_x = arrow_offset + arrow_side1_width;
			windata->drawn_arrow_end_x = arrow_offset + arrow_side1_width + arrow_side2_width;

			if (arrow_type == GTK_ARROW_UP)
			{
				windata->drawn_arrow_begin_y = DEFAULT_ARROW_HEIGHT;
				windata->drawn_arrow_middle_y = 0;
				windata->drawn_arrow_end_y = DEFAULT_ARROW_HEIGHT;

				if (arrow_side1_width == 0)
				{
					ADD_POINT(0, 0, 0, 0);
				}
				else
				{
					ADD_POINT(0, DEFAULT_ARROW_HEIGHT, 0, 0);

					if (arrow_offset > 0)
					{
						ADD_POINT(arrow_offset - (arrow_side2_width > 0 ? 0 : 1), DEFAULT_ARROW_HEIGHT, 0, 0);
					}

					ADD_POINT(arrow_offset + arrow_side1_width - (arrow_side2_width > 0 ? 0 : 1), 0, 0, 0);
				}

				if (arrow_side2_width > 0)
				{
					ADD_POINT(windata->drawn_arrow_end_x, windata->drawn_arrow_end_y, 1, 0);
					ADD_POINT(width - 1, DEFAULT_ARROW_HEIGHT, 1, 0);
				}

				ADD_POINT(width - 1, height - 1, 1, 1);
				ADD_POINT(0, height - 1, 0, 1);

				y = windata->point_y;
			}
			else
			{
				windata->drawn_arrow_begin_y = height - DEFAULT_ARROW_HEIGHT;
				windata->drawn_arrow_middle_y = height;
				windata->drawn_arrow_end_y = height - DEFAULT_ARROW_HEIGHT;

				ADD_POINT(0, 0, 0, 0);
				ADD_POINT(width - 1, 0, 1, 0);

				if (arrow_side2_width == 0)
				{
					ADD_POINT(width - 1, height, (arrow_side1_width > 0 ? 0 : 1), 0);
				}
				else
				{
					ADD_POINT(width - 1, height - DEFAULT_ARROW_HEIGHT, 1, 1);

					if (arrow_offset < width - arrow_side1_width)
					{
						ADD_POINT(arrow_offset + arrow_side1_width + arrow_side2_width, height - DEFAULT_ARROW_HEIGHT, 0, 1);
					}

					ADD_POINT(arrow_offset + arrow_side1_width, height, 0, 1);
				}

				if (arrow_side1_width > 0)
				{
					ADD_POINT(windata->drawn_arrow_begin_x - (arrow_side2_width > 0 ? 0 : 1), windata->drawn_arrow_begin_y, 0, 0);
					ADD_POINT(0, height - DEFAULT_ARROW_HEIGHT, 0, 1);
				}

				y = windata->point_y - height;
			}

			#if 0
				g_assert(i == windata->num_border_points);
				g_assert(windata->point_x - arrow_offset - arrow_side1_width >= 0);
			#endif

			gtk_window_move(GTK_WINDOW(windata->win), windata->point_x - arrow_offset - arrow_side1_width, y);

			break;

		case GTK_ARROW_LEFT:
		case GTK_ARROW_RIGHT:

			if (norm_point_y < arrow_side1_width)
			{
				arrow_side1_width = 0;
				arrow_offset = norm_point_y;
			}
			else if (norm_point_y > monitor_geometry.height - arrow_side2_width)
			{
				arrow_side2_width = 0;
				arrow_offset = norm_point_y - arrow_side1_width;
			}
			break;

		default:
			g_assert_not_reached();
	}

	g_assert(shape_points != NULL);

	/* FIXME won't work with GTK+3, need a replacement */
	/*windata->window_region = gdk_region_polygon(shape_points, windata->num_border_points, GDK_EVEN_ODD_RULE);*/
	g_free(shape_points);
}

static void draw_border(GtkWidget* widget, WindowData *windata, cairo_t* cr)
{
	cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0);
	cairo_set_line_width(cr, 1.0);

	if (windata->has_arrow)
	{
		size_t i;

		create_border_with_arrow(windata->win, windata);

		cairo_move_to(cr, windata->border_points[0].x + 0.5, windata->border_points[0].y + 0.5);

		for (i = 1; i < windata->num_border_points; i++)
		{
				cairo_line_to(cr, windata->border_points[i].x + 0.5, windata->border_points[i].y + 0.5);
		}

		cairo_close_path(cr);
		/* FIXME window_region is not set up anyway, see previous fixme */
		/*gdk_window_shape_combine_region (gtk_widget_get_window (windata->win), windata->window_region, 0, 0);*/
		g_free(windata->border_points);
		windata->border_points = NULL;
	}
	else
	{
		cairo_rectangle(cr, 0.5, 0.5, windata->width - 0.5, windata->height - 0.5);
	}

	cairo_stroke(cr);
}

static void
paint_window (GtkWidget  *widget,
	      cairo_t    *cr,
	      WindowData *windata)
{
	cairo_t*         cr2;
	cairo_surface_t* surface;
	GtkAllocation    allocation;

	gtk_widget_get_allocation(windata->win, &allocation);

	if (windata->width == 0)
	{
			windata->width = allocation.width;
			windata->height = allocation.height;
	}

	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);

	gtk_widget_get_allocation(widget, &allocation);

	surface = cairo_surface_create_similar (cairo_get_target (cr),
						CAIRO_CONTENT_COLOR_ALPHA,
						allocation.width,
						allocation.height);

	cr2 = cairo_create (surface);

	fill_background(widget, windata, cr2);
	draw_border(widget, windata, cr2);
	draw_stripe(widget, windata, cr2);
	cairo_fill (cr2);
	cairo_destroy (cr2);

	cairo_set_source_surface (cr, surface, 0, 0);
	cairo_paint(cr);
	cairo_surface_destroy(surface);
}

static gboolean
on_draw (GtkWidget *widget, cairo_t *cr, WindowData *windata)
{
	paint_window (widget, cr, windata);

	return FALSE;
}

static void destroy_windata(WindowData* windata)
{
	if (windata->window_region != NULL)
	{
		cairo_region_destroy(windata->window_region);
	}

	g_free(windata);
}

static void update_spacers(GtkWidget* nw)
{
	WindowData* windata;

	windata = g_object_get_data(G_OBJECT(nw), "windata");

	if (windata->has_arrow)
	{
		switch (get_notification_arrow_type(GTK_WIDGET(nw)))
		{
			case GTK_ARROW_UP:
				gtk_widget_show(windata->top_spacer);
				gtk_widget_hide(windata->bottom_spacer);
				break;

			case GTK_ARROW_DOWN:
				gtk_widget_hide(windata->top_spacer);
				gtk_widget_show(windata->bottom_spacer);
				break;

			default:
				g_assert_not_reached();
		}
	}
	else
	{
		gtk_widget_hide(windata->top_spacer);
		gtk_widget_hide(windata->bottom_spacer);
	}
}

static void update_content_hbox_visibility(WindowData* windata)
{
	/*
	 * This is all a hack, but until we have a libview-style ContentBox,
	 * it'll just have to do.
	 */
	if (gtk_widget_get_visible(windata->icon) || gtk_widget_get_visible(windata->body_label) || gtk_widget_get_visible(windata->actions_box))
	{
		gtk_widget_show(windata->content_hbox);
	}
	else
	{
		gtk_widget_hide(windata->content_hbox);
	}
}

static gboolean configure_event_cb(GtkWidget* nw, GdkEventConfigure* event, WindowData* windata)
{
	windata->width = event->width;
	windata->height = event->height;

	update_spacers(nw);
	gtk_widget_queue_draw(nw);

	return FALSE;
}

static gboolean activate_link(GtkLabel* label, const char* url, WindowData* windata)
{
	windata->url_clicked(GTK_WINDOW(windata->win), url);

	return TRUE;
}

GtkWindow* create_notification(UrlClickedCb url_clicked)
{
	GtkWidget* spacer;
	GtkWidget* win;
	GtkWidget* main_vbox;
	GtkWidget* hbox;
	GtkWidget* vbox;
	GtkWidget* close_button;
	GtkWidget* image;
	AtkObject* atkobj;
	WindowData* windata;

	GdkVisual *visual;
	GdkScreen* screen;

	windata = g_new0(WindowData, 1);
	windata->urgency = URGENCY_NORMAL;
	windata->url_clicked = url_clicked;

	win = gtk_window_new(GTK_WINDOW_POPUP);
	gtk_window_set_resizable(GTK_WINDOW(win), FALSE);
	windata->win = win;

	windata->composited = FALSE;

	screen = gtk_window_get_screen(GTK_WINDOW(win));

	visual = gdk_screen_get_rgba_visual(screen);

	if (visual != NULL)
	{
		gtk_widget_set_visual(win, visual);

		if (gdk_screen_is_composited(screen))
		{
			windata->composited = TRUE;
		}
	}

	gtk_window_set_title(GTK_WINDOW(win), "Notification");
	gtk_window_set_type_hint(GTK_WINDOW(win), GDK_WINDOW_TYPE_HINT_NOTIFICATION);
	gtk_widget_add_events(win, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
	gtk_widget_realize(win);
	gtk_widget_set_size_request(win, WIDTH, -1);

	g_object_set_data_full(G_OBJECT(win), "windata", windata, (GDestroyNotify) destroy_windata);
	atk_object_set_role(gtk_widget_get_accessible(win), ATK_ROLE_ALERT);

	g_signal_connect(G_OBJECT(win), "configure_event", G_CALLBACK(configure_event_cb), windata);

	main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_show(main_vbox);
	gtk_container_add (GTK_CONTAINER (win), main_vbox);
	gtk_container_set_border_width(GTK_CONTAINER(main_vbox), 1);

	g_signal_connect (G_OBJECT (main_vbox), "draw", G_CALLBACK (on_draw), windata);

	windata->top_spacer = gtk_image_new();
	gtk_box_pack_start(GTK_BOX(main_vbox), windata->top_spacer, FALSE, FALSE, 0);
	gtk_widget_set_size_request(windata->top_spacer, -1, DEFAULT_ARROW_HEIGHT);

	windata->main_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_show(windata->main_hbox);
	gtk_box_pack_start(GTK_BOX(main_vbox), windata->main_hbox, FALSE, FALSE, 0);

	windata->bottom_spacer = gtk_image_new();
	gtk_box_pack_start(GTK_BOX(main_vbox), windata->bottom_spacer, FALSE, FALSE, 0);
	gtk_widget_set_size_request(windata->bottom_spacer, -1, DEFAULT_ARROW_HEIGHT);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_show(vbox);
	gtk_box_pack_start(GTK_BOX(windata->main_hbox), vbox, TRUE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	spacer = gtk_image_new();
	gtk_widget_show(spacer);
	gtk_box_pack_start(GTK_BOX(hbox), spacer, FALSE, FALSE, 0);
	gtk_widget_set_size_request(spacer, SPACER_LEFT, -1);

	windata->summary_label = gtk_label_new(NULL);
	gtk_widget_show(windata->summary_label);
	gtk_box_pack_start(GTK_BOX(hbox), windata->summary_label, TRUE, TRUE, 0);
	gtk_label_set_xalign (GTK_LABEL (windata->summary_label), 0.0);
	gtk_label_set_yalign (GTK_LABEL (windata->summary_label), 0.0);
	gtk_label_set_line_wrap(GTK_LABEL(windata->summary_label), TRUE);
	gtk_label_set_line_wrap_mode (GTK_LABEL (windata->summary_label), PANGO_WRAP_WORD_CHAR);

	atkobj = gtk_widget_get_accessible(windata->summary_label);
	atk_object_set_description (atkobj, _("Notification summary text."));

	/* Add the close button */
	close_button = gtk_button_new();
	windata->close_button = close_button;
	gtk_widget_set_halign (close_button, GTK_ALIGN_END);
	gtk_widget_set_valign (close_button, GTK_ALIGN_START);
	gtk_widget_show(close_button);
	gtk_box_pack_start(GTK_BOX(hbox), close_button, FALSE, FALSE, 0);
	gtk_button_set_relief(GTK_BUTTON(close_button), GTK_RELIEF_NONE);
	gtk_container_set_border_width(GTK_CONTAINER(close_button), 0);
	//gtk_widget_set_size_request(close_button, 20, 20);
	g_signal_connect_swapped(G_OBJECT(close_button), "clicked", G_CALLBACK(gtk_widget_destroy), win);

	atkobj = gtk_widget_get_accessible(close_button);
	atk_action_set_description(ATK_ACTION(atkobj), 0,
                               _("Closes the notification."));
	atk_object_set_name(atkobj, "");
	atk_object_set_description (atkobj, _("Closes the notification."));

	image = gtk_image_new_from_icon_name ("window-close", GTK_ICON_SIZE_MENU);
	gtk_widget_show(image);
	gtk_container_add(GTK_CONTAINER(close_button), image);

	windata->content_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start(GTK_BOX(vbox), windata->content_hbox, FALSE, FALSE, 0);

	windata->iconbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_show(windata->iconbox);
	gtk_box_pack_start(GTK_BOX(windata->content_hbox), windata->iconbox, FALSE, FALSE, 0);
	gtk_widget_set_size_request(windata->iconbox, BODY_X_OFFSET, -1);

	windata->icon = gtk_image_new();
	gtk_box_pack_start(GTK_BOX(windata->iconbox), windata->icon, TRUE, TRUE, 0);
	gtk_widget_set_halign (windata->icon, GTK_ALIGN_CENTER);
	gtk_widget_set_valign (windata->icon, GTK_ALIGN_START);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_show(vbox);
	gtk_box_pack_start(GTK_BOX(windata->content_hbox), vbox, TRUE, TRUE, 0);

	windata->body_label = gtk_label_new(NULL);
	gtk_widget_show(windata->body_label);
	gtk_box_pack_start(GTK_BOX(vbox), windata->body_label, TRUE, TRUE, 0);
	gtk_label_set_xalign (GTK_LABEL (windata->body_label), 0.0);
	gtk_label_set_yalign (GTK_LABEL (windata->body_label), 0.0);
	gtk_label_set_line_wrap(GTK_LABEL(windata->body_label), TRUE);
	gtk_label_set_line_wrap_mode (GTK_LABEL (windata->body_label), PANGO_WRAP_WORD_CHAR);
    gtk_label_set_max_width_chars (GTK_LABEL (windata->body_label), 50);
	g_signal_connect(G_OBJECT(windata->body_label), "activate-link", G_CALLBACK(activate_link), windata);

	atkobj = gtk_widget_get_accessible(windata->body_label);
	atk_object_set_description (atkobj, _("Notification body text."));

	windata->actions_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_halign (windata->actions_box, GTK_ALIGN_END);
	gtk_widget_show(windata->actions_box);
	gtk_box_pack_start(GTK_BOX(vbox), windata->actions_box, FALSE, TRUE, 0);

	return GTK_WINDOW(win);
}

void set_notification_hints(GtkWindow *nw, GVariant *hints)
{
	WindowData *windata = g_object_get_data(G_OBJECT(nw), "windata");
	guint8 urgency;
	gboolean action_icons;

	g_assert(windata != NULL);

	if (g_variant_lookup(hints, "urgency", "y", &urgency))
	{
		windata->urgency = urgency;

		if (windata->urgency == URGENCY_CRITICAL) {
			gtk_window_set_title(GTK_WINDOW(nw), "Critical Notification");
		} else {
			gtk_window_set_title(GTK_WINDOW(nw), "Notification");
		}
	}

	/* Determine if action-icons have been requested */
	if (g_variant_lookup(hints, "action-icons", "b", &action_icons))
	{
		windata->action_icons = action_icons;
	}
}

void set_notification_timeout(GtkWindow* nw, glong timeout)
{
	WindowData* windata = g_object_get_data(G_OBJECT(nw), "windata");

	g_assert(windata != NULL);

	windata->timeout = timeout;
}

void notification_tick(GtkWindow* nw, glong remaining)
{
	WindowData* windata = g_object_get_data(G_OBJECT(nw), "windata");

	windata->remaining = remaining;

	if (windata->pie_countdown != NULL)
	{
		gtk_widget_queue_draw_area(windata->pie_countdown, 0, 0, PIE_WIDTH, PIE_HEIGHT);
	}
}

void set_notification_text(GtkWindow* nw, const char* summary, const char* body)
{
	char* str;
	size_t str_len;
	char* quoted;
	GtkRequisition req;
	WindowData* windata;

	windata = g_object_get_data(G_OBJECT(nw), "windata");
	g_assert(windata != NULL);

	quoted = g_markup_escape_text(summary, -1);
	str = g_strdup_printf("<b><big>%s</big></b>", quoted);
	g_free(quoted);

	gtk_label_set_markup(GTK_LABEL(windata->summary_label), str);
	g_free(str);

	/* body */
	xmlDocPtr doc;
	xmlInitParser();
	str = g_strconcat ("<markup>", body, "</markup>", NULL);
	/* parse notification body */
	str_len = strlen (str);
	doc = xmlReadMemory(str, (int) str_len, "noname.xml", NULL, 0);
	g_free (str);
	if (doc != NULL) {
		xmlXPathContextPtr xpathCtx;
		xmlXPathObjectPtr  xpathObj;
		xmlNodeSetPtr      nodes;
		const char        *body_label_text;
		int i, size;

		/* filterout img nodes */
		xpathCtx = xmlXPathNewContext(doc);
		xpathObj = xmlXPathEvalExpression((unsigned char *)"//img", xpathCtx);
		nodes = xpathObj->nodesetval;
		size = (nodes) ? nodes->nodeNr : 0;
		for(i = size - 1; i >= 0; i--) {
			xmlUnlinkNode (nodes->nodeTab[i]);
			xmlFreeNode (nodes->nodeTab[i]);
		}

		/* write doc to string */
		xmlBufferPtr buf = xmlBufferCreate();
		(void) xmlNodeDump(buf, doc, xmlDocGetRootElement (doc), 0, 0);
		str = (char *)buf->content;
		gtk_label_set_markup (GTK_LABEL (windata->body_label), str);

		/* cleanup */
		xmlBufferFree (buf);
		xmlXPathFreeObject (xpathObj);
		xmlXPathFreeContext (xpathCtx);
		xmlFreeDoc (doc);

		/* Does it render properly? */
		body_label_text = gtk_label_get_text (GTK_LABEL (windata->body_label));
		if ((body_label_text == NULL) || (strlen (body_label_text) == 0)) {
			goto render_fail;
		}
		goto render_ok;
	}

render_fail:
	/* could not parse notification body */
	quoted = g_markup_escape_text(body, -1);
	gtk_label_set_markup (GTK_LABEL (windata->body_label), quoted);
	g_free (quoted);

render_ok:
	xmlCleanupParser ();

	if (body == NULL || *body == '\0')
		gtk_widget_hide(windata->body_label);
	else
		gtk_widget_show(windata->body_label);

	update_content_hbox_visibility(windata);

	if (body != NULL && *body != '\0')
	{
		gtk_widget_get_preferred_size (windata->iconbox, NULL, &req);
		/* -1: border width for
		 * -6: spacing for hbox */
		gtk_widget_set_size_request(windata->body_label, WIDTH - (1 * 2) - (10 * 2) - req.width - 6, -1);
	}

		gtk_widget_get_preferred_size (windata->close_button, NULL, &req);
	/* -1: main_vbox border width
	 * -10: vbox border width
	 * -6: spacing for hbox */
	gtk_widget_set_size_request(windata->summary_label, WIDTH - (1 * 2) - (10 * 2) - SPACER_LEFT - req.width - (6 * 2), -1);
}

void set_notification_icon(GtkWindow* nw, GdkPixbuf* pixbuf)
{
	WindowData* windata = g_object_get_data(G_OBJECT(nw), "windata");

	g_assert(windata != NULL);

	gtk_image_set_from_pixbuf(GTK_IMAGE(windata->icon), pixbuf);

	if (pixbuf != NULL)
	{
		int pixbuf_width = gdk_pixbuf_get_width(pixbuf);

		gtk_widget_show(windata->icon);
		gtk_widget_set_size_request(windata->iconbox, MAX(BODY_X_OFFSET, pixbuf_width), -1);
	}
	else
	{
		gtk_widget_hide(windata->icon);
		gtk_widget_set_size_request(windata->iconbox, BODY_X_OFFSET, -1);
	}

	update_content_hbox_visibility(windata);
}

void set_notification_arrow(GtkWidget* nw, gboolean visible, int x, int y)
{
	WindowData* windata = g_object_get_data(G_OBJECT(nw), "windata");

	g_assert(windata != NULL);

	windata->has_arrow = visible;
	windata->point_x = x;
	windata->point_y = y;

	update_spacers(nw);
}

static void
paint_countdown (GtkWidget  *pie,
                 cairo_t    *cr,
                 WindowData *windata)
{
    GtkStyleContext *context;
    GdkRGBA bg;
    GtkAllocation alloc;
    cairo_t* cr2;
    cairo_surface_t* surface;

    context = gtk_widget_get_style_context (windata->win);

    gtk_style_context_save (context);
    gtk_style_context_set_state (context, GTK_STATE_FLAG_SELECTED);

    get_background_color (context, GTK_STATE_FLAG_SELECTED, &bg);

    gtk_style_context_restore (context);

    gtk_widget_get_allocation(pie, &alloc);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    surface = cairo_surface_create_similar (cairo_get_target(cr),
                                            CAIRO_CONTENT_COLOR_ALPHA,
                                            alloc.width,
                                            alloc.height);

    cr2 = cairo_create (surface);

    fill_background (pie, windata, cr2);

    if (windata->timeout > 0)
    {
        gdouble pct = (gdouble) windata->remaining / (gdouble) windata->timeout;

        gdk_cairo_set_source_rgba (cr2, &bg);

        cairo_move_to (cr2, PIE_RADIUS, PIE_RADIUS);
        cairo_arc_negative (cr2, PIE_RADIUS, PIE_RADIUS, PIE_RADIUS, -G_PI_2, -(pct * G_PI * 2) - G_PI_2);
        cairo_line_to (cr2, PIE_RADIUS, PIE_RADIUS);
        cairo_fill (cr2);
    }

    cairo_destroy(cr2);

    cairo_save (cr);
    cairo_set_source_surface (cr, surface, 0, 0);
    cairo_paint (cr);
    cairo_restore (cr);

    cairo_surface_destroy(surface);
}

static gboolean
on_countdown_draw (GtkWidget *widget, cairo_t *cr, WindowData *windata)
{
	paint_countdown (widget, cr, windata);

	return FALSE;
}

static void action_clicked_cb(GtkWidget* w, GdkEventButton* event, ActionInvokedCb action_cb)
{
	GtkWindow* nw;
	const char* key;
	nw = g_object_get_data(G_OBJECT(w), "_nw");
	key = g_object_get_data(G_OBJECT(w), "_action_key");
	action_cb(nw, key);
}

void add_notification_action(GtkWindow* nw, const char* text, const char* key, ActionInvokedCb cb)
{
	WindowData* windata;
	GtkWidget* label;
	GtkWidget* button;
	GtkWidget* hbox;
	GdkPixbuf* pixbuf;
	char* buf;

	windata = g_object_get_data(G_OBJECT(nw), "windata");

	g_assert(windata != NULL);

	if (gtk_widget_get_visible(windata->actions_box))
	{
		gtk_widget_show(windata->actions_box);
		update_content_hbox_visibility(windata);

		/* Don't try to re-add a pie_countdown */
		if (!windata->pie_countdown) {
			windata->pie_countdown = gtk_drawing_area_new();
			gtk_widget_set_halign (windata->pie_countdown, GTK_ALIGN_END);
			gtk_widget_show(windata->pie_countdown);

			gtk_box_pack_end (GTK_BOX (windata->actions_box), windata->pie_countdown, FALSE, TRUE, 0);
			gtk_widget_set_size_request(windata->pie_countdown,
						    PIE_WIDTH, PIE_HEIGHT);
			g_signal_connect(G_OBJECT(windata->pie_countdown), "draw",
					 G_CALLBACK(on_countdown_draw), windata);
		}
	}

	if (windata->action_icons) {
		button = gtk_button_new_from_icon_name(key, GTK_ICON_SIZE_BUTTON);
		goto add_button;
	}

	button = gtk_button_new();
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_show(hbox);
	gtk_container_add(GTK_CONTAINER(button), hbox);

	/* Try to be smart and find a suitable icon. */
	buf = g_strdup_printf("stock_%s", key);
	pixbuf = gtk_icon_theme_load_icon(gtk_icon_theme_get_for_screen(gdk_window_get_screen(gtk_widget_get_window(GTK_WIDGET(nw)))),
																	buf, 16, GTK_ICON_LOOKUP_USE_BUILTIN, NULL);
	g_free(buf);

	if (pixbuf != NULL)
	{
		GtkWidget* image = gtk_image_new_from_pixbuf(pixbuf);
		gtk_widget_show(image);
		gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);
		gtk_widget_set_halign (image, GTK_ALIGN_CENTER);
		gtk_widget_set_valign (image, GTK_ALIGN_CENTER);
	}

	label = gtk_label_new(NULL);
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_label_set_xalign (GTK_LABEL (label), 0.0);
	gtk_label_set_yalign (GTK_LABEL (label), 0.5);
	buf = g_strdup_printf("<small>%s</small>", text);
	gtk_label_set_markup(GTK_LABEL(label), buf);
	g_free(buf);

add_button:
	gtk_widget_show(button);
	gtk_box_pack_start(GTK_BOX(windata->actions_box), button, FALSE, FALSE, 0);

	g_object_set_data(G_OBJECT(button), "_nw", nw);
	g_object_set_data_full(G_OBJECT(button), "_action_key", g_strdup(key), g_free);
	g_signal_connect(G_OBJECT(button), "button-release-event", G_CALLBACK(action_clicked_cb), cb);

	gtk_widget_show_all(windata->actions_box);
}

void clear_notification_actions(GtkWindow* nw)
{
	WindowData* windata = g_object_get_data(G_OBJECT(nw), "windata");

	windata->pie_countdown = NULL;

	gtk_widget_hide(windata->actions_box);
	gtk_container_foreach(GTK_CONTAINER(windata->actions_box), (GtkCallback) gtk_widget_destroy, NULL);
}

void move_notification(GtkWidget* nw, int x, int y)
{
	WindowData* windata = g_object_get_data(G_OBJECT(nw), "windata");

	g_assert(windata != NULL);

	if (windata->has_arrow)
	{
		gtk_widget_queue_resize(nw);
	}
	else
	{
		gtk_window_move(GTK_WINDOW(nw), x, y);
	}
}

void get_theme_info(char** theme_name, char** theme_ver, char** author, char** homepage)
{
	*theme_name = g_strdup("Standard");

	/* If they are constants, maybe we can remove printf and use G_STRINGIFY() */
	*theme_ver = g_strdup_printf("%d.%d.%d", NOTIFICATION_DAEMON_MAJOR_VERSION, NOTIFICATION_DAEMON_MINOR_VERSION, NOTIFICATION_DAEMON_MICRO_VERSION);
	*author = g_strdup("Christian Hammond");
	*homepage = g_strdup("http://www.galago-project.org/");
}

gboolean theme_check_init(unsigned int major_ver, unsigned int minor_ver, unsigned int micro_ver)
{
	return major_ver == NOTIFICATION_DAEMON_MAJOR_VERSION && minor_ver == NOTIFICATION_DAEMON_MINOR_VERSION && micro_ver == NOTIFICATION_DAEMON_MICRO_VERSION;
}
