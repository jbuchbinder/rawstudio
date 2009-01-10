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

/* Plugin tmpl version 4 */

#include <rawstudio.h>

#define RS_TYPE_INPUT_IMAGE16 (rs_input_image16_type)
#define RS_INPUT_IMAGE16(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_INPUT_IMAGE16, RSInputImage16))
#define RS_INPUT_IMAGE16_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_INPUT_IMAGE16, RSInputImage16Class))
#define RS_IS_INPUT_IMAGE16(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_INPUT_IMAGE16))

typedef struct _RSInputImage16 RSInputImage16;
typedef struct _RSInputImage16Class RSInputImage16Class;

struct _RSInputImage16 {
	RSFilter parent;

	RS_IMAGE16 *image;
	gulong signal;
};

struct _RSInputImage16Class {
	RSFilterClass parent_class;
};

RS_DEFINE_FILTER(rs_input_image16, RSInputImage16)

enum {
	PROP_0,
	PROP_IMAGE
};

static void get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static RS_IMAGE16 *get_image(RSFilter *filter);
static gint get_width(RSFilter *filter);
static gint get_height(RSFilter *filter);
static void image_changed(RS_IMAGE16 *image, RSInputImage16 *input_image16);

static RSFilterClass *rs_input_image16_parent_class = NULL;

G_MODULE_EXPORT void
rs_plugin_load(RSPlugin *plugin)
{
	/* Let the GType system register our type */
	rs_input_image16_get_type(G_TYPE_MODULE(plugin));
}

static void
rs_input_image16_class_init (RSInputImage16Class *klass)
{
	RSFilterClass *filter_class = RS_FILTER_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	rs_input_image16_parent_class = g_type_class_peek_parent (klass);

	object_class->get_property = get_property;
	object_class->set_property = set_property;

	g_object_class_install_property(object_class,
		PROP_IMAGE, g_param_spec_object (
			"image",
			"image",
			"RS_IMAGE16 to import",
			RS_TYPE_IMAGE16,
			G_PARAM_READWRITE)
	);

	filter_class->name = "Import a RS_IMAGE16 into a RSFilter chain";
	filter_class->get_image = get_image;
	filter_class->get_width = get_width;
	filter_class->get_height = get_height;
}

static void
rs_input_image16_init (RSInputImage16 *input_image16)
{
	input_image16->image = NULL;
	input_image16->signal = 0;
}

static void
get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	RSInputImage16 *input_image16 = RS_INPUT_IMAGE16(object);
	switch (property_id)
	{
		case PROP_IMAGE:
			g_value_set_object(value, input_image16->image);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	RSInputImage16 *input_image16 = RS_INPUT_IMAGE16(object);
	switch (property_id)
	{
		case PROP_IMAGE:
			if (input_image16->signal)
				g_signal_handler_disconnect(input_image16->image, input_image16->signal);
			if (input_image16->image)
				g_object_unref(input_image16->image);
			input_image16->image = g_object_ref(g_value_get_object(value));
			input_image16->signal = g_signal_connect(G_OBJECT(input_image16->image), "pixeldata-changed", G_CALLBACK(image_changed), input_image16);
			rs_filter_changed(RS_FILTER(input_image16));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static RS_IMAGE16 *
get_image(RSFilter *filter)
{
	RSInputImage16 *input_image16 = RS_INPUT_IMAGE16(filter);

	if (!RS_IS_IMAGE16(input_image16->image))
		return NULL;

	return g_object_ref(input_image16->image);
}

static gint
get_width(RSFilter *filter)
{
	RSInputImage16 *input_image16 = RS_INPUT_IMAGE16(filter);

	if (!RS_IS_IMAGE16(input_image16->image))
		return -1;

	return input_image16->image->w;
}

static gint
get_height(RSFilter *filter)
{
	RSInputImage16 *input_image16 = RS_INPUT_IMAGE16(filter);

	if (!RS_IS_IMAGE16(input_image16->image))
		return -1;

	return input_image16->image->h;
}

static void
image_changed(RS_IMAGE16 *image, RSInputImage16 *input_image16)
{
	rs_filter_changed(RS_FILTER(input_image16));
}
