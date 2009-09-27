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

/* Documentation:
 * http://www.sqlite.org/capi3ref.html
 */

/* Database layout:
 *
 * library 
 *   id
 *   filename
 *
 * tags
 *   id
 *   tagname
 *
 * phototags
 *   photo
 *   tag
 */

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <string.h>
#include "rawstudio.h"
#include "rs-metadata.h"
#include "rs-library.h"

void library_sqlite_error(sqlite3 *db, gint result);
gint library_create_tables(sqlite3 *db);
gint library_find_tag_id(RS_LIBRARY *library, gchar *tagname);
gint library_find_photo_id(RS_LIBRARY *library, gchar *photo);
void library_photo_add_tag(RS_LIBRARY *library, gint photo_id, gint tag_id, gboolean autotag);
gboolean library_is_photo_tagged(RS_LIBRARY *library, gint photo_id, gint tag_id);
void library_add_photo(RS_LIBRARY *library, gchar *filename);
void library_add_tag(RS_LIBRARY *library, gchar *tagname);
void library_delete_photo(RS_LIBRARY *library, gint photo_id);
void library_delete_tag(RS_LIBRARY *library, gint tag_id);
void library_photo_delete_tags(RS_LIBRARY *library, gint photo_id);
void library_tag_delete_photos(RS_LIBRARY *library, gint tag_id);
gboolean library_tag_is_used(RS_LIBRARY *library, gint tag_id);

//GList* rs_library_search(GList *tags);
//void rs_library_add_directory(gchar *directory, gboolean recursive);


/* BEGIN PRIVATE FUNCTIONS */

void
library_sqlite_error(sqlite3 *db, gint result)
{
	if (result != SQLITE_OK)
	{
		g_warning("sqlite3 warning: %s\n", sqlite3_errmsg(db));
	}
}

gint
library_create_tables(sqlite3 *db)
{
	sqlite3_stmt *stmt;
	gint rc;
       
	/* Create table (library) to hold all known photos */
	sqlite3_prepare_v2(db, "create table library (id integer primary key, filename varchar(1024))", -1, &stmt, NULL);
	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	/* Create table (tags) with all known tags */
	sqlite3_prepare_v2(db, "create table tags (id integer primary key, tagname varchar(128))", -1, &stmt, NULL);
	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	/* Create table (phototags) to bind tags and photos together */
	sqlite3_prepare_v2(db, "create table phototags (photo integer, tag integer, autotag integer)", -1, &stmt, NULL);
	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	return SQLITE_OK;
}


gint
library_find_tag_id(RS_LIBRARY *library, gchar *tagname)
{
	sqlite3 *db = library->db;
	sqlite3_stmt *stmt;
	gint rc, tag_id = -1;

	rc = sqlite3_prepare_v2(db, "SELECT id FROM tags WHERE tagname = ?1;", -1, &stmt, NULL);
	rc = sqlite3_bind_text(stmt, 1, tagname, strlen(tagname), SQLITE_TRANSIENT);
	rc = sqlite3_step(stmt);
	if (rc == SQLITE_ROW)
		tag_id = sqlite3_column_int(stmt, 0);
	rc = sqlite3_finalize(stmt);
	return tag_id;
}

gint
library_find_photo_id(RS_LIBRARY *library, gchar *photo)
{
	sqlite3 *db = library->db;
	sqlite3_stmt *stmt;
	gint rc, photo_id = -1;

	rc = sqlite3_prepare_v2(db, "SELECT id FROM library WHERE filename = ?1;", -1, &stmt, NULL);
	rc = sqlite3_bind_text(stmt, 1, photo, strlen(photo), SQLITE_TRANSIENT);
	library_sqlite_error(db, rc);
	rc = sqlite3_step(stmt);
	if (rc == SQLITE_ROW)
		photo_id = sqlite3_column_int(stmt, 0);
	rc = sqlite3_finalize(stmt);
	return photo_id;
}

void
library_photo_add_tag(RS_LIBRARY *library, gint photo_id, gint tag_id, gboolean autotag)
{
	sqlite3 *db = library->db;
	gint rc;
	sqlite3_stmt *stmt;

	gint autotag_tag = 0;
	if (autotag)
		autotag_tag = 1;

	rc = sqlite3_prepare_v2(db, "INSERT INTO phototags (photo, tag, autotag) VALUES (?1, ?2, ?3);", -1, &stmt, NULL);
	rc = sqlite3_bind_int (stmt, 1, photo_id);
	rc = sqlite3_bind_int (stmt, 2, tag_id);
	rc = sqlite3_bind_int (stmt, 3, autotag_tag);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE)
		library_sqlite_error(db, rc);
	sqlite3_finalize(stmt);
}

gboolean
library_is_photo_tagged(RS_LIBRARY *library, gint photo_id, gint tag_id)
{
	sqlite3 *db = library->db;
	gint rc;
	sqlite3_stmt *stmt;

	rc = sqlite3_prepare_v2(db, "SELECT * FROM phototags WHERE photo = ?1 AND tag = ?2;", -1, &stmt, NULL);
	rc = sqlite3_bind_int (stmt, 1, photo_id);
	rc = sqlite3_bind_int (stmt, 2, tag_id);
	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	if (rc == SQLITE_ROW)
		return TRUE;
	else
		return FALSE;
}

void
library_add_photo(RS_LIBRARY *library, gchar *filename)
{
	sqlite3 *db = library->db;
	gint rc;
	sqlite3_stmt *stmt;

	sqlite3_prepare_v2(db, "INSERT INTO library (filename) VALUES (?1);", -1, &stmt, NULL);
	rc = sqlite3_bind_text(stmt, 1, filename, strlen(filename), SQLITE_TRANSIENT);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE)
		library_sqlite_error(db, rc);
	sqlite3_finalize(stmt);
}

void
library_add_tag(RS_LIBRARY *library, gchar *tagname)
{
	sqlite3 *db = library->db;
	gint rc;
	sqlite3_stmt *stmt;

	sqlite3_prepare_v2(db, "INSERT INTO tags (tagname) VALUES (?1);", -1, &stmt, NULL);
	rc = sqlite3_bind_text(stmt, 1, tagname, strlen(tagname), SQLITE_TRANSIENT);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE)
		library_sqlite_error(db, rc);
	sqlite3_finalize(stmt);
}

void 
library_delete_photo(RS_LIBRARY *library, gint photo_id)
{
	sqlite3 *db = library->db;
	sqlite3_stmt *stmt;
	gint rc;

	rc = sqlite3_prepare_v2(db, "DELETE FROM library WHERE id = ?1;", -1, &stmt, NULL);
	rc = sqlite3_bind_int(stmt, 1, photo_id);
	library_sqlite_error(db, rc);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE)
		library_sqlite_error(db, rc);
	rc = sqlite3_finalize(stmt);
}

void 
library_delete_tag(RS_LIBRARY *library, gint tag_id)
{
	sqlite3 *db = library->db;
	sqlite3_stmt *stmt;
	gint rc;

	rc = sqlite3_prepare_v2(db, "DELETE FROM library WHERE filename = ?1;", -1, &stmt, NULL);
	rc = sqlite3_bind_int(stmt, 1, tag_id);
	library_sqlite_error(db, rc);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE)
		library_sqlite_error(db, rc);
	rc = sqlite3_finalize(stmt);
}

void 
library_photo_delete_tags(RS_LIBRARY *library, gint photo_id)
{
	sqlite3 *db = library->db;
	sqlite3_stmt *stmt;
	gint rc;

	rc = sqlite3_prepare_v2(db, "DELETE FROM phototags WHERE photo = ?1;", -1, &stmt, NULL);
	rc = sqlite3_bind_int(stmt, 1, photo_id);
	library_sqlite_error(db, rc);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE)
		library_sqlite_error(db, rc);
	rc = sqlite3_finalize(stmt);
}

void
library_tag_delete_photos(RS_LIBRARY *library, gint tag_id)
{
	sqlite3 *db = library->db;
	sqlite3_stmt *stmt;
	gint rc;

	rc = sqlite3_prepare_v2(db, "DELETE FROM phototags WHERE tag = ?1;", -1, &stmt, NULL);
	rc = sqlite3_bind_int(stmt, 1, tag_id);
	library_sqlite_error(db, rc);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE)
		library_sqlite_error(db, rc);
	rc = sqlite3_finalize(stmt);
}

gboolean
library_tag_is_used(RS_LIBRARY *library, gint tag_id)
{
	sqlite3 *db = library->db;
	gint rc;
	sqlite3_stmt *stmt;

	rc = sqlite3_prepare_v2(db, "SELECT * FROM phototags WHERE tag = ?1;", -1, &stmt, NULL);
	rc = sqlite3_bind_int (stmt, 1, tag_id);
	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	if (rc == SQLITE_ROW)
		return TRUE;
	else
		return FALSE;
}

/* END PRIVATE FUNCTIONS */


/* BEGIN PUBLIC FUNCTIONS */

RS_LIBRARY *
rs_library_new()
{
	return g_malloc(sizeof(RS_LIBRARY));
}

void
rs_library_init(RS_LIBRARY *library)
{
	int rc;

	gchar *database = g_strdup_printf("%s/.rawstudio/library.db", g_get_home_dir());

	/* If unable to create database we exit */
	if(sqlite3_open(database, &(library->db)))
	{
		g_debug("sqlite3 debug: could not open database %s\n", database);
		sqlite3_close(library->db);
		exit(1);
	}
	g_free(database);

	rc = library_create_tables(library->db);
	library_sqlite_error(library->db, rc);
}

void
rs_library_destroy(RS_LIBRARY *library)
{
	sqlite3_close(library->db);
}

void
rs_library_add_photo(RS_LIBRARY *library, gchar *filename)
{
	if (library_find_photo_id(library, filename) == -1)
	{
		g_debug("Adding photo to library: %s",filename);
		library_add_photo(library, filename);
	}
}

void
rs_library_add_tag(RS_LIBRARY *library, gchar *tagname)
{
	if (library_find_tag_id(library, tagname) == -1)
	{
		g_debug("Adding tag to tags: %s",tagname);
		library_add_tag(library, tagname);
	}

}

void
rs_library_photo_add_tag(RS_LIBRARY *library, gchar *filename, gchar *tagname, gboolean autotag)
{
	gint photo_id = 0, tag_id;

	photo_id = library_find_photo_id(library, filename);
	if (photo_id == -1)
	{
		g_warning("Photo not known...");
		return;
	}

	tag_id = library_find_tag_id(library, tagname);
	if (tag_id == -1)
	{
		g_warning("Tag not known...");
		return;
	}

	if (!library_is_photo_tagged(library, photo_id, tag_id))
		library_photo_add_tag(library, photo_id, tag_id, autotag);

	return;
}

void
rs_library_delete_photo(RS_LIBRARY *library, gchar *photo)
{
	gint photo_id = -1;

	photo_id = library_find_photo_id(library, photo);
	if (photo_id == -1)
	{
		g_warning("Photo not known...");
		return;
	}

	library_photo_delete_tags(library, photo_id);
	library_delete_photo(library, photo_id);
}

gboolean
rs_library_delete_tag(RS_LIBRARY *library, gchar *tag, gboolean force)
{
	gint tag_id = -1;

	tag_id = library_find_tag_id(library, tag);
	if (tag_id == -1)
	{
		g_warning("Tag not known...");
		return FALSE;
	}

	if (library_tag_is_used(library, tag_id))
		if (force)
		{
			library_tag_delete_photos(library, tag_id);
			library_delete_tag(library, tag_id);
		}
		else
		{
			g_warning("Tag is in use...");
			return FALSE;
		}
	else
		library_delete_tag(library, tag_id);
	return TRUE;
}

GList *
rs_library_search(RS_LIBRARY *library, GList *tags)
{
	sqlite3_stmt *stmt;
	gint rc;
	sqlite3 *db = library->db;
	gchar *tag;
	gint n, num_tags = g_list_length(tags);
	GList *photos = NULL;
	GTimer *gt = g_timer_new();
	
	sqlite3_prepare_v2(db, "create temp table filter (photo integer)", -1, &stmt, NULL);
	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
       
	for (n = 0; n < num_tags; n++)
	{
		tag = (gchar *) g_list_nth_data(tags, n);

		sqlite3_prepare_v2(db, "insert into filter select phototags.photo from phototags, tags where phototags.tag = tags.id and tags.tagname = ?1;", -1, &stmt, NULL);
		rc = sqlite3_bind_text(stmt, 1, tag, strlen(tag), SQLITE_TRANSIENT);
		rc = sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}

	sqlite3_prepare_v2(db, "create temp table result (photo integer, count integer)", -1, &stmt, NULL);
	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	sqlite3_prepare_v2(db, "insert into result select photo, count(photo) from filter group by photo;", -1, &stmt, NULL);
	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	sqlite3_prepare_v2(db, "select library.filename from library,result where library.id = result.photo and result.count = ?1 order by library.filename;", -1, &stmt, NULL);
        rc = sqlite3_bind_int(stmt, 1, num_tags);
	while (sqlite3_step(stmt) == SQLITE_ROW)
		photos = g_list_append(photos, g_strdup((gchar *) sqlite3_column_text(stmt, 0)));
	sqlite3_finalize(stmt);

	g_debug("Search in library took %.03f seconds", g_timer_elapsed(gt, NULL));
	g_timer_destroy(gt);

	return photos;
}

void
rs_library_photo_default_tags(RS_LIBRARY *library, gchar *photo, RSMetadata *metadata)
{
	rs_library_add_photo(library, photo);
	if (metadata->make_ascii)
	{
		rs_library_add_tag(library, metadata->make_ascii);
		rs_library_photo_add_tag(library, photo, metadata->make_ascii, TRUE);
	}
	if (metadata->model_ascii)
	{
		rs_library_add_tag(library, metadata->model_ascii);
		rs_library_photo_add_tag(library, photo, metadata->model_ascii, TRUE);
	}
	if (metadata->lens_min_focal != -1 && metadata->lens_max_focal != -1)
	{
		gchar *lens = NULL;
		if (metadata->lens_min_focal == metadata->lens_max_focal)
			lens = g_strdup_printf("%dmm",(gint) metadata->lens_min_focal);
		else
			lens = g_strdup_printf("%d-%dmm",(gint) metadata->lens_min_focal, (gint) metadata->lens_max_focal);
		rs_library_add_tag(library, lens);
		rs_library_photo_add_tag(library, photo, lens, TRUE);
		g_free(lens);

	}
	if (metadata->focallength != -1)
	{
		gchar *text = NULL;
		if (metadata->focallength < 50)
			text = g_strdup("wideangle");
		else
			text = g_strdup("telephoto");
		rs_library_add_tag(library, text);
		rs_library_photo_add_tag(library, photo, text, TRUE);
		g_free(text);
	}
	if (metadata->timestamp != -1)
	{
		gchar *year = NULL;
		gchar *month = NULL;
		GDate *date = g_date_new();
		g_date_set_time_t(date, metadata->timestamp);
		year = g_strdup_printf("%d", g_date_get_year(date));
		gint m = g_date_get_month(date);

		switch (m)
		{
		case 1:
			month = g_strdup("January");
			break;
		case 2:
			month = g_strdup("February");
			break;
		case 3:
			month = g_strdup("March");
			break;
		case 4:
			month = g_strdup("April");
			break;
		case 5:
			month = g_strdup("May");
			break;
		case 6:
			month = g_strdup("June");
			break;
		case 7:
			month = g_strdup("July");
			break;
		case 8:
			month = g_strdup("August");
			break;
		case 9:
			month = g_strdup("September");
			break;
		case 10:
			month = g_strdup("Ocotober");
			break;
		case 11:
			month = g_strdup("November");
			break;
		case 12:
			month = g_strdup("December");
			break;
		}

		rs_library_add_tag(library, year);
		rs_library_photo_add_tag(library, photo, year, TRUE);
		rs_library_add_tag(library, month);
		rs_library_photo_add_tag(library, photo, month, TRUE);

		g_date_free(date);
		g_free(year);
		g_free(month);
	}

}

GList *
rs_library_photo_tags(RS_LIBRARY *library, gchar *photo, gboolean autotag)
{
	sqlite3_stmt *stmt;
	gint rc;
	sqlite3 *db = library->db;
	GList *tags = NULL;

	if (autotag)
	{
		sqlite3_prepare_v2(db, "select tags.tagname from library,phototags,tags WHERE library.id=phototags.photo and phototags.tag=tags.id and library.filename = ?1;", -1, &stmt, NULL);
		rc = sqlite3_bind_text(stmt, 1, photo, strlen(photo), NULL);
	}
	else
	{
		sqlite3_prepare_v2(db, "select tags.tagname from library,phototags,tags WHERE library.id=phototags.photo and phototags.tag=tags.id and library.filename = ?1 and phototags.autotag = 0;", -1, &stmt, NULL);
		rc = sqlite3_bind_text(stmt, 1, photo, strlen(photo), NULL);
	}
	while (sqlite3_step(stmt) == SQLITE_ROW)
		tags = g_list_append(tags, g_strdup((gchar *) sqlite3_column_text(stmt, 0)));
	sqlite3_finalize(stmt);

	return tags;
}

GList *
rs_library_find_tag(RS_LIBRARY *library, gchar *tag)
{
	sqlite3_stmt *stmt;
	gint rc;
	sqlite3 *db = library->db;
	GList *tags = NULL;

	rc = sqlite3_prepare_v2(db, "select tags.tagname from tags WHERE tags.tagname like ?1 order by tags.tagname;", -1, &stmt, NULL);
	gchar *like = g_strdup_printf("%%%s%%", tag);
        rc = sqlite3_bind_text(stmt, 1, like, strlen(like), NULL);
	library_sqlite_error(db, rc);
	
	while (sqlite3_step(stmt) == SQLITE_ROW)
		tags = g_list_append(tags, g_strdup((gchar *) sqlite3_column_text(stmt, 0)));
	sqlite3_finalize(stmt);

	g_free(like);

	return tags;
}

/* END PUBLIC FUNCTIONS */
