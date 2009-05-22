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

#ifndef RS_LENS_DB_H
#define RS_LENS_DB_H

#include <glib-object.h>
#include "rs-lens.h"

G_BEGIN_DECLS

#define RS_TYPE_LENS_DB rs_lens_db_get_type()
#define RS_LENS_DB(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_LENS_DB, RSLensDb))
#define RS_LENS_DB_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_LENS_DB, RSLensDbClass))
#define RS_IS_LENS_DB(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_LENS_DB))
#define RS_IS_LENS_DB_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RS_TYPE_LENS_DB))
#define RS_LENS_DB_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_TYPE_LENS_DB, RSLensDbClass))
GType rs_lens_db_get_type(void);

typedef struct _RSLensDb RSLensDb;

typedef struct {
	GObjectClass parent_class;
} RSLensDbClass;

/**
 * Instantiate a new RSLensDb
 * @param path An absolute path to a XML-file containing the database
 * @return A new RSLensDb with a refcount of 1
 */
RSLensDb *
rs_lens_db_new(const char *path);

/**
 * Get the default RSLensDb as used globally by Rawstudio
 * @return A new RSLensDb, this should not be unref'ed after use!
 */
RSLensDb *rs_lens_db_get_default(void);

/**
 * Look up identifer in database
 * @param lens_db A RSLensDb to search in
 * @param identifier A lens identifier as generated by metadata subsystem
 */
RSLens *rs_lens_db_get_from_identifier(RSLensDb *lens_db, const gchar *identifier);

/**
 * Add a lens to the database - will only be added if the lens appear unique
 * @param lens_db A RSLensDb
 * @param lens A RSLens to add
 */
void *rs_lens_db_add_lens(RSLensDb *lens_db, RSLens *lens);

/**
 * Lookup a lens in the database based on information in a RSMetadata
 * @param lens_db A RSLensDb
 * @param metadata A RSMetadata
 * @return A RSLens or NULL if unsuccesful
 */
RSLens *rs_lens_db_lookup_from_metadata(RSLensDb *lens_db, RSMetadata *metdata);

G_END_DECLS

#endif /* RS_LENS_DB_H */
