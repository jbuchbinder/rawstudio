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

#ifndef RS_LIBRARY_H
#define RS_LIBRARY_H

#include <glib.h>
#include <rawstudio.h>
#include "sqlite3.h"

typedef struct
{
	sqlite3 *db;
} RS_LIBRARY;

void rs_library_init(RS_LIBRARY *library);
void rs_library_add_photo(RS_LIBRARY *library, gchar *filename);
void rs_library_add_tag(RS_LIBRARY *library, gchar *tagname);
void rs_library_photo_add_tag(RS_LIBRARY *library, gchar *filename, gchar *tagname);
void rs_library_delete_photo(RS_LIBRARY *library, gchar *photo);
gboolean rs_library_delete_tag(RS_LIBRARY *library, gchar *tag, gboolean force);

#endif /* RS_LIBRARY_H */
