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
	GtkWidget* main_hbox;
	GtkWidget* icon;
	GtkWidget* content_hbox;
	GtkWidget* summary_label;
	GtkWidget* close_button;
	GtkWidget* body_label;
	GtkWidget* actions_box;
	GtkWidget* last_sep;
	GtkWidget* pie_countdown;

	gboolean has_arrow;
	gboolean composited;
	gboolean action_icons;

	int width;
	int height;
	int last_width;
	int last_height;

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
gboolean get_always_stack(GtkWidget* nw);

#define WIDTH          400
#define DEFAULT_X0     0
#define DEFAULT_Y0     0
#define DEFAULT_RADIUS 8
#define IMAGE_SIZE     48
#define PIE_RADIUS     8
#define PIE_WIDTH      (2 * PIE_RADIUS)
#define PIE_HEIGHT     (2 * PIE_RADIUS)
#define BODY_X_OFFSET  (IMAGE_SIZE + 4)
#define BACKGROUND_ALPHA    0.90

#define MAX_ICON_SIZE IMAGE_SIZE

static void draw_round_rect(cairo_t* cr, gdouble aspect, gdouble x, gdouble y, gdouble corner_radius, gdouble width, gdouble height)
{
	gdouble radius = corner_radius / aspect;

	cairo_move_to(cr, x + radius, y);

	// top-right, left of the corner
	cairo_line_to(cr, x + width - radius, y);

	// top-right, below the corner
	cairo_arc(cr, x + width - radius, y + radius, radius, -90.0f * G_PI / 180.0f, 0.0f * G_PI / 180.0f);

	// bottom-right, above the corner
	cairo_line_to(cr, x + width, y + height - radius);

	// bottom-right, left of the corner
	cairo_arc(cr, x + width - radius, y + height - radius, radius, 0.0f * G_PI / 180.0f, 90.0f * G_PI / 180.0f);

	// bottom-left, right of the corner
	cairo_line_to(cr, x + radius, y + height);

	// bottom-left, above the corner
	cairo_arc(cr, x + radius, y + height - radius, radius, 90.0f * G_PI / 180.0f, 180.0f * G_PI / 180.0f);

	// top-left, below the corner
	cairo_line_to(cr, x, y + radius);

	// top-left, right of the corner
	cairo_arc(cr, x + radius, y + radius, radius, 180.0f * G_PI / 180.0f, 270.0f * G_PI / 180.0f);
}

static void
get_background_color (GtkStyleContext *context,
                      GtkStateFlags    state,
                      GdkRGBA         *color)
{
    GdkRGBA *c;

    g_return_if_fail (color != NULL);
    g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

    gtk_style_context_get (context,
                           state,
                           "background-color", &c,
                           NULL);
    *color = *c;
    gdk_rgba_free (c);
}

static void fill_background(GtkWidget* widget, WindowData* windata, cairo_t* cr)
{
	GtkAllocation allocation;
	GtkStyleContext *context;
	GdkRGBA fg;
	GdkRGBA bg;

	gtk_widget_get_allocation(widget, &allocation);

	draw_round_rect(cr, 1.0f, DEFAULT_X0 + 1, DEFAULT_Y0 + 1, DEFAULT_RADIUS, allocation.width - 2, allocation.height - 2);

	context = gtk_widget_get_style_context(widget);

	gtk_style_context_save (context);
	gtk_style_context_set_state (context, GTK_STATE_FLAG_NORMAL);

	get_background_color (context, GTK_STATE_FLAG_NORMAL, &bg);
	gtk_style_context_get_color (context, GTK_STATE_FLAG_NORMAL, &fg);

	gtk_style_context_restore (context);

	cairo_set_source_rgba(cr, bg.red, bg.green, bg.blue, BACKGROUND_ALPHA);
	cairo_fill_preserve(cr);

	/* Should we show urgency somehow?  Probably doesn't
	 * have any meaningful value to the user... */

	cairo_set_source_rgba(cr, fg.red, fg.green, fg.blue, BACKGROUND_ALPHA);
	cairo_set_line_width(cr, 1);
	cairo_stroke(cr);
}

static void
update_shape_region (cairo_surface_t *surface,
                     WindowData      *windata)
{
	if (windata->width == windata->last_width && windata->height == windata->last_height)
	{
		return;
	}

	if (windata->width == 0 || windata->height == 0)
	{
		GtkAllocation allocation;
		gtk_widget_get_allocation (windata->win, &allocation);

		windata->width = MAX (allocation.width, 1);
		windata->height = MAX (allocation.height, 1);
	}

	if (!windata->composited) {
		cairo_region_t *region;

		region = gdk_cairo_region_create_from_surface (surface);
		gtk_widget_shape_combine_region (windata->win, region);
		cairo_region_destroy (region);
	} else {
		gtk_widget_shape_combine_region (windata->win, NULL);
		return;
	}

	windata->last_width = windata->width;
	windata->last_height = windata->height;
}

static void paint_window (GtkWidget  *widget,
			  cairo_t    *cr,
			  WindowData *windata)
{
	cairo_surface_t *surface;
	cairo_t *cr2;
		GtkAllocation allocation;

		gtk_widget_get_allocation (windata->win, &allocation);

	if (windata->width == 0 || windata->height == 0)
	{
		windata->width = MAX (allocation.width, 1);
		windata->height = MAX (allocation.height, 1);
	}

	surface = cairo_surface_create_similar (cairo_get_target (cr),
						CAIRO_CONTENT_COLOR_ALPHA,
						windata->width,
						windata->height);

	cr2 = cairo_create (surface);

	/* transparent background */
	cairo_rectangle (cr2, 0, 0, windata->width, windata->height);
	cairo_set_source_rgba (cr2, 0.0, 0.0, 0.0, 0.0);
	cairo_fill (cr2);

	fill_background (widget, windata, cr2);

	cairo_destroy(cr2);

	cairo_save (cr);
	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_surface(cr, surface, 0, 0);
	cairo_paint(cr);
	update_shape_region (surface, windata);
	cairo_restore (cr);

	cairo_surface_destroy(surface);
}

static gboolean on_window_map(GtkWidget* widget, GdkEvent* event, WindowData* windata)
{
	return FALSE;
}

static gboolean
on_draw (GtkWidget  *widget,
	 cairo_t    *cr,
	 WindowData *windata)
{
	paint_window (widget, cr, windata);

	return FALSE;
}

static void destroy_windata(WindowData* windata)
{
	g_free(windata);
}

static void update_content_hbox_visibility(WindowData* windata)
{
	if (gtk_widget_get_visible(windata->icon) || gtk_widget_get_visible(windata->body_label) || gtk_widget_get_visible(windata->actions_box))
	{
		gtk_widget_show(windata->content_hbox);
	}
	else
	{
		gtk_widget_hide(windata->content_hbox);
	}
}

static gboolean on_configure_event(GtkWidget* widget, GdkEventConfigure* event, WindowData* windata)
{
	windata->width = event->width;
	windata->height = event->height;

	gtk_widget_queue_draw(widget);

	return FALSE;
}

static void on_window_realize(GtkWidget* widget, WindowData* windata)
{
	/* Nothing */
}

static void on_composited_changed(GtkWidget* window, WindowData* windata)
{
	windata->composited = gdk_screen_is_composited(gtk_widget_get_screen(window));

	gtk_widget_queue_draw (windata->win);
}

GtkWindow* create_notification(UrlClickedCb url_clicked)
{
	GtkWidget* win;
	GtkWidget* main_vbox;
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
	gtk_widget_set_app_paintable(win, TRUE);
	g_signal_connect(G_OBJECT(win), "map-event", G_CALLBACK(on_window_map), windata);
	g_signal_connect(G_OBJECT(win), "draw", G_CALLBACK(on_draw), windata);
	g_signal_connect(G_OBJECT(win), "realize", G_CALLBACK(on_window_realize), windata);

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

	g_signal_connect(win, "composited-changed", G_CALLBACK(on_composited_changed), windata);

	gtk_window_set_title(GTK_WINDOW(win), "Notification");
	gtk_window_set_type_hint(GTK_WINDOW(win), GDK_WINDOW_TYPE_HINT_NOTIFICATION);
	gtk_widget_add_events(win, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);

	g_object_set_data_full(G_OBJECT(win), "windata", windata, (GDestroyNotify) destroy_windata);
	atk_object_set_role(gtk_widget_get_accessible(win), ATK_ROLE_ALERT);

	g_signal_connect(G_OBJECT(win), "configure-event", G_CALLBACK(on_configure_event), windata);

	main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_show(main_vbox);
	gtk_container_add(GTK_CONTAINER(win), main_vbox);
	gtk_container_set_border_width(GTK_CONTAINER(main_vbox), 12);

	windata->main_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_show(windata->main_hbox);
	gtk_box_pack_start(GTK_BOX(main_vbox), windata->main_hbox, FALSE, FALSE, 0);

	/* Add icon */
	windata->icon = gtk_image_new();
	gtk_widget_set_valign (windata->icon, GTK_ALIGN_START);
	gtk_widget_set_margin_top (windata->icon, 5);
	gtk_widget_set_size_request (windata->icon, BODY_X_OFFSET, -1);
	gtk_widget_show(windata->icon);
	gtk_box_pack_start (GTK_BOX(windata->main_hbox), windata->icon, FALSE, FALSE, 0);

	/* Add vbox */
	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_show(vbox);
	gtk_box_pack_start(GTK_BOX(windata->main_hbox), vbox, TRUE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);

	/* Add the close button */
	close_button = gtk_button_new();
	gtk_widget_set_valign (close_button, GTK_ALIGN_START);
	gtk_widget_show(close_button);

	windata->close_button = close_button;
	gtk_box_pack_start (GTK_BOX (windata->main_hbox),
                        windata->close_button,
                        FALSE, FALSE, 0);

	gtk_button_set_relief(GTK_BUTTON(close_button), GTK_RELIEF_NONE);
	gtk_container_set_border_width(GTK_CONTAINER(close_button), 0);
	g_signal_connect_swapped(G_OBJECT(close_button), "clicked", G_CALLBACK(gtk_widget_destroy), win);

	atkobj = gtk_widget_get_accessible(close_button);
	atk_action_set_description(ATK_ACTION(atkobj), 0,
                                          _("Closes the notification."));
	atk_object_set_name(atkobj, "");
	atk_object_set_description (atkobj, _("Closes the notification."));

	image = gtk_image_new_from_icon_name ("window-close", GTK_ICON_SIZE_MENU);
	gtk_widget_show(image);
	gtk_container_add(GTK_CONTAINER(close_button), image);

	/* center vbox */
	windata->summary_label = gtk_label_new(NULL);
	gtk_widget_show(windata->summary_label);
	gtk_box_pack_start(GTK_BOX(vbox), windata->summary_label, TRUE, TRUE, 0);
	gtk_label_set_xalign (GTK_LABEL (windata->summary_label), 0.0);
	gtk_label_set_yalign (GTK_LABEL (windata->summary_label), 0.0);
	gtk_label_set_line_wrap(GTK_LABEL(windata->summary_label), TRUE);
	gtk_label_set_line_wrap_mode (GTK_LABEL (windata->summary_label), PANGO_WRAP_WORD_CHAR);

	atkobj = gtk_widget_get_accessible(windata->summary_label);
	atk_object_set_description (atkobj, _("Notification summary text."));

	windata->content_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_show(windata->content_hbox);
	gtk_box_pack_start(GTK_BOX(vbox), windata->content_hbox, FALSE, FALSE, 0);

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
	g_signal_connect_swapped(G_OBJECT(windata->body_label), "activate-link", G_CALLBACK(windata->url_clicked), win);

	atkobj = gtk_widget_get_accessible(windata->body_label);
	atk_object_set_description (atkobj, _("Notification body text."));

	windata->actions_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_halign (windata->actions_box, GTK_ALIGN_END);
	gtk_widget_show(windata->actions_box);

	gtk_box_pack_start (GTK_BOX (vbox), windata->actions_box, FALSE, TRUE, 0);

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

void set_notification_timeout(GtkWindow *nw, glong timeout)
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
	int summary_width;

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

	gtk_widget_get_preferred_size (windata->close_button, NULL, &req);
	/* -1: main_vbox border width
	   -10: vbox border width
	   -6: spacing for hbox */
	summary_width = WIDTH - (1 * 2) - (10 * 2) - BODY_X_OFFSET - req.width - (6 * 2);

	if (body != NULL && *body != '\0')
	{
		gtk_widget_set_size_request(windata->body_label, summary_width, -1);
	}

	gtk_widget_set_size_request(windata->summary_label, summary_width, -1);
}

static GdkPixbuf* scale_pixbuf(GdkPixbuf* pixbuf, int max_width, int max_height, gboolean no_stretch_hint)
{
	float scale_factor_x = 1.0;
	float scale_factor_y = 1.0;
	float scale_factor = 1.0;

	int pw = gdk_pixbuf_get_width(pixbuf);
	int ph = gdk_pixbuf_get_height(pixbuf);

	/* Determine which dimension requires the smallest scale. */
	scale_factor_x = (float) max_width / (float) pw;
	scale_factor_y = (float) max_height / (float) ph;

	if (scale_factor_x > scale_factor_y)
	{
		scale_factor = scale_factor_y;
	}
	else
	{
		scale_factor = scale_factor_x;
	}

	/* always scale down, allow to disable scaling up */
	if (scale_factor < 1.0 || !no_stretch_hint)
	{
		int scale_x;
		int scale_y;

		scale_x = (int) (((float) pw) * scale_factor);
		scale_y = (int) (((float) ph) * scale_factor);

		return gdk_pixbuf_scale_simple(pixbuf, scale_x, scale_y, GDK_INTERP_BILINEAR);
	}
	else
	{
		return g_object_ref(pixbuf);
	}
}

void set_notification_icon(GtkWindow* nw, GdkPixbuf* pixbuf)
{
	WindowData* windata = g_object_get_data(G_OBJECT(nw), "windata");

	g_assert(windata != NULL);

	GdkPixbuf* scaled = NULL;

	if (pixbuf != NULL)
	{
		scaled = scale_pixbuf(pixbuf, MAX_ICON_SIZE, MAX_ICON_SIZE, TRUE);
	}

	gtk_image_set_from_pixbuf(GTK_IMAGE(windata->icon), scaled);

	if (scaled != NULL)
	{
		int pixbuf_width = gdk_pixbuf_get_width(scaled);

		gtk_widget_show(windata->icon);
		gtk_widget_set_size_request(windata->icon, MAX(BODY_X_OFFSET, pixbuf_width), -1);
		g_object_unref(scaled);
	}
	else
	{
		gtk_widget_hide(windata->icon);

		gtk_widget_set_size_request(windata->icon, BODY_X_OFFSET, -1);
	}

	update_content_hbox_visibility(windata);
}

void set_notification_arrow(GtkWidget* nw, gboolean visible, int x, int y)
{
	WindowData* windata = g_object_get_data(G_OBJECT(nw), "windata");

	g_assert(windata != NULL);
}

static void
paint_countdown (GtkWidget  *pie,
                 cairo_t* cr,
                 WindowData* windata)
{
	GtkStyleContext* context;
	GdkRGBA bg;
	GtkAllocation allocation;
	cairo_t* cr2;
	cairo_surface_t* surface;

	context = gtk_widget_get_style_context(windata->win);

	gtk_style_context_save (context);
	gtk_style_context_set_state (context, GTK_STATE_FLAG_SELECTED);

	get_background_color (context, GTK_STATE_FLAG_SELECTED, &bg);

	gtk_style_context_restore (context);

	gtk_widget_get_allocation(pie, &allocation);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	surface = cairo_surface_create_similar(cairo_get_target(cr),
                                           CAIRO_CONTENT_COLOR_ALPHA,
                                           allocation.width,
                                           allocation.height);

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

static void on_action_clicked(GtkWidget* w, GdkEventButton *event, ActionInvokedCb action_cb)
{
	GtkWindow* nw = g_object_get_data(G_OBJECT(w), "_nw");
	const char* key = g_object_get_data(G_OBJECT(w), "_action_key");

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

	if (!gtk_widget_get_visible(windata->actions_box))
	{
		gtk_widget_show(windata->actions_box);
		update_content_hbox_visibility(windata);

		/* Don't try to re-add a pie_countdown */
		if (!windata->pie_countdown) {
			windata->pie_countdown = gtk_drawing_area_new();
			gtk_widget_set_halign (windata->pie_countdown, GTK_ALIGN_END);
			gtk_widget_set_valign (windata->pie_countdown, GTK_ALIGN_CENTER);
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
	gtk_widget_show(button);

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
	gtk_box_pack_start(GTK_BOX(windata->actions_box), button, FALSE, FALSE, 0);
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	gtk_container_set_border_width(GTK_CONTAINER(button), 0);

	g_object_set_data(G_OBJECT(button), "_nw", nw);
	g_object_set_data_full(G_OBJECT(button), "_action_key", g_strdup(key), g_free);
	g_signal_connect(G_OBJECT(button), "button-release-event", G_CALLBACK(on_action_clicked), cb);

	gtk_widget_show_all(windata->actions_box);
}

void clear_notification_actions(GtkWindow* nw)
{
	WindowData* windata = g_object_get_data(G_OBJECT(nw), "windata");

	windata->pie_countdown = NULL;

	gtk_widget_hide(windata->actions_box);
	gtk_container_foreach(GTK_CONTAINER(windata->actions_box), (GtkCallback) gtk_widget_destroy, NULL);
}

void move_notification(GtkWidget* widget, int x, int y)
{
	WindowData* windata = g_object_get_data(G_OBJECT(widget), "windata");

	g_assert(windata != NULL);

	gtk_window_move(GTK_WINDOW(windata->win), x, y);
}

void get_theme_info(char** theme_name, char** theme_ver, char** author, char** homepage)
{
	*theme_name = g_strdup("Slider");
	*theme_ver  = g_strdup_printf("%d.%d.%d", NOTIFICATION_DAEMON_MAJOR_VERSION, NOTIFICATION_DAEMON_MINOR_VERSION, NOTIFICATION_DAEMON_MICRO_VERSION);
	*author = g_strdup("William Jon McCann");
	*homepage = g_strdup("http://www.gnome.org/");
}

gboolean get_always_stack(GtkWidget* nw)
{
	return TRUE;
}

gboolean theme_check_init(unsigned int major_ver, unsigned int minor_ver, unsigned int micro_ver)
{
	return major_ver == NOTIFICATION_DAEMON_MAJOR_VERSION && minor_ver == NOTIFICATION_DAEMON_MINOR_VERSION && micro_ver == NOTIFICATION_DAEMON_MICRO_VERSION;
}
