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

#include <rawstudio.h>
#include "rs-filter.h"

G_DEFINE_TYPE (RSFilter, rs_filter, G_TYPE_OBJECT)

static void
rs_filter_class_init(RSFilterClass *klass)
{
	g_debug("rs_filter_class_init(%p)", klass);
}

static void
rs_filter_init(RSFilter *self)
{
	g_debug("rs_filter_init(%p)", self);
}

/**
 * Return a new instance of a RSFilter
 * @param name The name of the filter
 * @param previous The previous filter or NULL
 * @return The newly instantiated RSFilter or NULL
 */
RSFilter *
rs_filter_new(const gchar *name, RSFilter *previous)
{
	g_debug("rs_filter_new(%s, %p)", name, previous);
	g_assert(name != NULL);
	g_assert((previous == NULL) || RS_IS_FILTER(previous));

	GType type = g_type_from_name(name);
	RSFilter *filter = NULL;

	if (g_type_is_a (type, RS_TYPE_FILTER))
		filter = g_object_new(type, NULL);

	if (filter)
		rs_filter_set_previous(filter, previous);

	return filter;
}

/**
 * Set the previous RSFilter in a RSFilter-chain
 * @param filter A RSFilter
 * @param previous A previous RSFilter or NULL
 */
void
rs_filter_set_previous(RSFilter *filter, RSFilter *previous)
{
	g_debug("rs_filter_set_previous(%p, %p)", filter, previous);
	g_assert(RS_IS_FILTER(filter));
	g_assert(RS_IS_FILTER(previous));

	filter->previous = previous;
}

/**
 * Get the output image from a RSFilter
 * @param filter A RSFilter
 * @return A RSImage, this must be unref'ed
 */
RSImage *
rs_filter_get_image(RSFilter *filter)
{
	g_debug("rs_filter_get_image(%p)", filter);
	RSImage *image;
	g_assert(RS_IS_FILTER(filter));

	if (RS_FILTER_GET_CLASS(filter)->get_image)
		image = RS_FILTER_GET_CLASS(filter)->get_image(filter);
	else
		image = rs_filter_get_image(filter->previous);

	g_assert(RS_IS_IMAGE(image));

	return image;
}

/**
 * Get the returned width of a RSFilter
 * @param filter A RSFilter
 * @return Width in pixels
 */
gint
rs_filter_get_width(RSFilter *filter)
{
	gint width;
	g_assert(RS_IS_FILTER(filter));

	if (RS_FILTER_GET_CLASS(filter)->get_width)
		width = RS_FILTER_GET_CLASS(filter)->get_width(filter);
	else
		width = rs_filter_get_width(filter->previous);

	return width;
}

/**
 * Get the returned height of a RSFilter
 * @param filter A RSFilter
 * @return Height in pixels
 */
gint
rs_filter_get_height(RSFilter *filter)
{
	gint height;
	g_assert(RS_IS_FILTER(filter));

	if (RS_FILTER_GET_CLASS(filter)->get_height)
		height = RS_FILTER_GET_CLASS(filter)->get_height(filter);
	else
		height = rs_filter_get_height(filter->previous);

	return height;
}