/*
 * Copyright (C) 2006-2008 Anders Brander <anders@brander.dk> and 
 * Anders Kvist <akv@lnxbx.dk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/* Plugin tmpl version 1 */

#include <rawstudio.h>

#define RS_TYPE_GTK_VIEW (rs_gtk_view_type)
#define RS_GTK_VIEW(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_GTK_VIEW, RSGtkView))
#define RS_GTK_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_GTK_VIEW, RSGtkViewClass))
#define RS_IS_GTK_VIEW(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_GTK_VIEW))

typedef struct _RSGtkView RSGtkView;
typedef struct _RSGtkViewClass RSGtkViewClass;

struct _RSGtkView {
	RSFilter parent;

	GdkPixbuf *pixbuf;
	gchar *changeme;
	GtkWidget *drawing_area;
};

struct _RSGtkViewClass {
	RSFilterClass parent_class;
};

enum {
	PROP_0,
	PROP_CHANGEME
};

static GType rs_gtk_view_get_type (GTypeModule *module);
static void rs_gtk_view_class_init(RSGtkViewClass *klass);
static void rs_gtk_view_init(RSGtkView *filter);
static void get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void previous_changed(RSFilter *filter, RSFilter *parent);
static gboolean expose(GtkWidget *widget, GdkEventExpose *event, gpointer user_data);

static GType rs_gtk_view_type = 0;

static RSFilterClass *rs_gtk_view_parent_class = NULL;

G_MODULE_EXPORT void
rs_plugin_load(RSPlugin *plugin)
{
	/* Let the GType system register our type */
	rs_gtk_view_get_type(G_TYPE_MODULE(plugin));
}

static GType
rs_gtk_view_get_type (GTypeModule *module)
{
	if (!rs_gtk_view_type)
	{
		static const GTypeInfo filter_info =
		{
			sizeof (RSGtkViewClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) rs_gtk_view_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data     */
			sizeof (RSGtkView),
			0,              /* n_preallocs    */
			(GInstanceInitFunc) rs_gtk_view_init
		};

		rs_gtk_view_type = g_type_module_register_type(
			module,
			RS_TYPE_FILTER,
			"RSGtkView",
			&filter_info,
			0);
	}

	return rs_gtk_view_type;
}

static void
rs_gtk_view_class_init (RSGtkViewClass *klass)
{
	RSFilterClass *filter_class = RS_FILTER_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	rs_gtk_view_parent_class = g_type_class_peek_parent (klass);

	object_class->get_property = get_property;
	object_class->set_property = set_property;

	g_object_class_install_property(object_class,
		PROP_CHANGEME, g_param_spec_string (
			"changeme",
			"Changeme",
			"Changeme",
			NULL,
			G_PARAM_READWRITE)
	);

	filter_class->name = "GtkView";

	filter_class->previous_changed = previous_changed;
}

static void
rs_gtk_view_init (RSGtkView *gtk_view)
{
	GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_view->drawing_area = gtk_drawing_area_new();
	GtkWidget *viewport = gtk_viewport_new(NULL, NULL);
	GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);

	g_signal_connect(gtk_view->drawing_area, "expose-event", G_CALLBACK(expose), gtk_view);

//	RSFilter *prev = rs_filter_get_previous(RS_FILTER(gtk_view));
//	g_debug("prev = %p", prev);
	gtk_container_add(GTK_CONTAINER(viewport), gtk_view->drawing_area);
	gtk_container_add(GTK_CONTAINER(scrolled), viewport);
	gtk_container_add(GTK_CONTAINER(window), scrolled);
	gtk_widget_show_all(window);
	gtk_view->pixbuf = NULL;
	gtk_view->changeme = NULL;
}

static void
get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	switch (property_id)
	{
		case PROP_CHANGEME:
			g_value_get_string (value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	RSGtkView *gtk_view = RS_GTK_VIEW(object);
	switch (property_id)
	{
		case PROP_CHANGEME:
			g_free(gtk_view->changeme);
			gtk_view->changeme = g_value_dup_string(value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
previous_changed(RSFilter *filter, RSFilter *parent)
{
	RSGtkView *gtk_view = RS_GTK_VIEW(filter);
	gint width = rs_filter_get_width(parent);
	gint height = rs_filter_get_height(parent);
	RSColorTransform *rct = rs_color_transform_new();
	RS_IMAGE16 *image = rs_filter_get_image(parent);
	gfloat pre_mul[4] = {2.2, 1.5, 1.9, 1.5};

	if (gtk_view->pixbuf)
		g_object_unref(gtk_view->pixbuf);
	gtk_view->pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, width, height);


	GdkWindow *window = gtk_view->drawing_area->window;

	rs_color_transform_set_premul(rct, pre_mul);

	rs_color_transform_transform(rct,
		image->w,
		image->h,
		image->pixels,
		image->rowstride,
		gdk_pixbuf_get_pixels(gtk_view->pixbuf),
		gdk_pixbuf_get_rowstride(gtk_view->pixbuf));

	gtk_widget_set_size_request(gtk_view->drawing_area, width, height);
	gdk_draw_pixbuf(window, NULL, gtk_view->pixbuf, 0, 0, 0, 0, width, height, GDK_RGB_DITHER_NONE, 0, 0);
}

static gboolean
expose(GtkWidget *widget, GdkEventExpose *event, gpointer user_data)
{
	RSGtkView *gtk_view = RS_GTK_VIEW(user_data);
	GdkRectangle area = event->area;
	GdkWindow *window = gtk_view->drawing_area->window;

	if (gtk_view->pixbuf)
		gdk_draw_pixbuf(window, NULL, gtk_view->pixbuf, area.x, area.y, area.x, area.y, area.width, area.height, GDK_RGB_DITHER_NONE, 0, 0);

	return TRUE;
}
