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

#ifndef RS_FILTER_H
#define RS_FILTER_H

#include "rawstudio.h"
#include <glib-object.h>

G_BEGIN_DECLS

#define RS_TYPE_FILTER rs_filter_get_type()
#define RS_FILTER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_FILTER, RSFilter))
#define RS_FILTER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_FILTER, RSFilterClass))
#define RS_IS_FILTER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_FILTER))
#define RS_IS_FILTER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RS_TYPE_FILTER))
#define RS_FILTER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_TYPE_FILTER, RSFilterClass))

typedef struct _RSFilter RSFilter;
typedef struct _RSFilterClass RSFilterClass;

struct _RSFilter {
	GObject parent;
	RSFilter *previous;
};

struct _RSFilterClass {
	GObjectClass parent_class;
	const gchar *name;
	RSImage *(*get_image)(RSFilter *filter);
	gint (*get_width)(RSFilter *filter);
	gint (*get_height)(RSFilter *filter);
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
 * Get the output image from a RSFilter
 * @param filter A RSFilter
 * @return A RSImage, this must be unref'ed
 */
extern RSImage *rs_filter_get_image(RSFilter *filter);

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