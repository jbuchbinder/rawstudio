/*
 * Copyright (C) 2006-2009 Anders Brander <anders@brander.dk> and 
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

#ifndef RS_FILTER_H
#define RS_FILTER_H

#include "rawstudio.h"
#include <glib-object.h>

G_BEGIN_DECLS

/**
 * Convenience macro to define generic filter
 */
#define RS_DEFINE_FILTER(type_name, TypeName) \
static GType type_name##_get_type (GTypeModule *module); \
static void type_name##_class_init(TypeName##Class *klass); \
static void type_name##_init(TypeName *filter); \
static GType type_name##_type = 0; \
static GType \
type_name##_get_type(GTypeModule *module) \
{ \
	if (!type_name##_type) \
	{ \
		static const GTypeInfo filter_info = \
		{ \
			sizeof (TypeName##Class), \
			(GBaseInitFunc) NULL, \
			(GBaseFinalizeFunc) NULL, \
			(GClassInitFunc) type_name##_class_init, \
			NULL, \
			NULL, \
			sizeof (TypeName), \
			0, \
			(GInstanceInitFunc) type_name##_init \
		}; \
 \
		type_name##_type = g_type_module_register_type( \
			module, \
			RS_TYPE_FILTER, \
			#TypeName, \
			&filter_info, \
			0); \
	} \
	return type_name##_type; \
}

#define RS_TYPE_FILTER rs_filter_get_type()
#define RS_FILTER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_FILTER, RSFilter))
#define RS_FILTER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_FILTER, RSFilterClass))
#define RS_IS_FILTER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_FILTER))
#define RS_IS_FILTER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RS_TYPE_FILTER))
#define RS_FILTER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_TYPE_FILTER, RSFilterClass))

#define RS_FILTER_NAME(filter) (((filter)) ? g_type_name(G_TYPE_FROM_CLASS(RS_FILTER_GET_CLASS ((filter)))) : "(nil)")

typedef struct _RSFilter RSFilter;
typedef struct _RSFilterClass RSFilterClass;

struct _RSFilter {
	GObject parent;
	RSFilter *previous;
	GSList *next_filters;
};

struct _RSFilterClass {
	GObjectClass parent_class;
	const gchar *name;
	RS_IMAGE16 *(*get_image)(RSFilter *filter);
	GdkPixbuf *(*get_image8)(RSFilter *filter);
	gint (*get_width)(RSFilter *filter);
	gint (*get_height)(RSFilter *filter);
	void (*previous_changed)(RSFilter *filter, RSFilter *parent);
};

GType rs_filter_get_type() G_GNUC_CONST;

/**
 * Return a new instance of a RSFilter
 * @param name The name of the filter
 * @param previous The previous filter or NULL
 * @return The newly instantiated RSFilter or NULL
 */
extern RSFilter *rs_filter_new(const gchar *name, RSFilter *previous);

/**
 * Set the previous RSFilter in a RSFilter-chain
 * @param filter A RSFilter
 * @param previous A previous RSFilter or NULL
 */
extern void rs_filter_set_previous(RSFilter *filter, RSFilter *previous);

/**
 * Signal that a filter has changed, filters depending on this will be invoked
 * This should only be called from filters
 * @param filter The changed filter
 */
extern void rs_filter_changed(RSFilter *filter);

/**
 * Get the output image from a RSFilter
 * @param filter A RSFilter
 * @return A RS_IMAGE16, this must be unref'ed
 */
extern RS_IMAGE16 *rs_filter_get_image(RSFilter *filter);

/**
 * Get 8 bit output image from a RSFilter
 * @param filter A RSFilter
 * @return A RS_IMAGE16, this must be unref'ed
 */
GdkPixbuf *
rs_filter_get_image8(RSFilter *filter);

/**
 * Get the returned width of a RSFilter
 * @param filter A RSFilter
 * @return Width in pixels
 */
extern gint rs_filter_get_width(RSFilter *filter);

/**
 * Get the returned height of a RSFilter
 * @param filter A RSFilter
 * @return Height in pixels
 */
extern gint rs_filter_get_height(RSFilter *filter);

G_END_DECLS

#endif /* RS_FILTER_H */