/*
 * * Copyright (C) 2006-2011 Anders Brander <anders@brander.dk>, 
 * * Anders Kvist <akv@lnxbx.dk> and Klaus Post <klauspost@gmail.com>
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
 * This is standard "Toxi"-layout for taging.
 *
 * library: Known photos
 *   id integer primary key: Photo serial
 *   filename varchar(1024): Path to photo file
 *   identifier varchar(32): "rawstudio-sum" of the photo
 *
 * tags: Known tags
 *   id integer primary key: Tag serial
 *   tagname varchar(128): Tag name
 *
 * phototags: Mapping between photos and tags
 *   photo integer: An id from library->id
 *   tag integer: An id from tags->id
 *   autotag integer: 1 if the tag were added automaticly, 0 otherwise
 *
 * version: Version information for upgrading
 *   version integer: Version written by Rawstudio, can be compared to LIBRARY_VERSION
 */
/*
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <string.h>
#include "rawstudio.h"
#include "rs-metadata.h"
#include "rs-library.h"
#include "application.h"
*/

#include "rs-library.h"
#include "conf_interface.h"
#include "config.h"
#include "gettext.h"
#include "rs-debug.h"
#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
#include <sqlite3.h>

#define LIBRARY_VERSION 2
#define TAGS_XML_FILE "tags.xml"
#define MAX_SEARCH_RESULTS 1000
#include "rs-types.h"

struct _RSLibrary {
	GObject parent;
	gboolean dispose_has_run;

	sqlite3 *db;
	gchar *error_init;

	/* This mutex must be used when inserting data in a table with an
	   autocrementing column - which is ALWAYS for sqlite */
	GMutex *id_lock;
};

G_DEFINE_TYPE(RSLibrary, rs_library, G_TYPE_OBJECT)

static gint library_execute_sql(sqlite3 *db, const gchar *sql);
static void library_sqlite_error(sqlite3 *db, const gint result);
static gint library_create_tables(sqlite3 *db);
static gint library_find_tag_id(RSLibrary *library, const gchar *tagname);
static gint library_find_photo_id(RSLibrary *library, const gchar *photo);
static void library_photo_add_tag(RSLibrary *library, const gint photo_id, const gint tag_id, const gboolean autotag);
static gboolean library_is_photo_tagged(RSLibrary *library, const gint photo_id, const gint tag_id);
static gint library_add_photo(RSLibrary *library, const gchar *filename);
static gint library_add_tag(RSLibrary *library, const gchar *tagname);
static void library_delete_photo(RSLibrary *library, const gint photo_id);
static void library_delete_tag(RSLibrary *library, const gint tag_id);
static void library_photo_delete_tags(RSLibrary *library, const gint photo_id);
static void library_tag_delete_photos(RSLibrary *library, const gint tag_id);
static gboolean library_tag_is_used(RSLibrary *library, const gint tag_id);
static void library_photo_default_tags(RSLibrary *library, const gint photo_id, RSMetadata *metadata);

static GtkWidget *tag_search_entry = NULL;

static void
rs_library_dispose(GObject *object)
{
	RSLibrary *library = RS_LIBRARY(object);

	if (!library->dispose_has_run)
	{
		library->dispose_has_run = TRUE;

		sqlite3_close(library->db);

		g_mutex_free(library->id_lock);
	}

	G_OBJECT_CLASS(rs_library_parent_class)->dispose (object);
}

static void
rs_library_finalize(GObject *object)
{
	G_OBJECT_CLASS(rs_library_parent_class)->finalize (object);
}

static void
rs_library_class_init(RSLibraryClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	sqlite3_config(SQLITE_CONFIG_SERIALIZED);
	object_class->dispose = rs_library_dispose;
	object_class->finalize = rs_library_finalize;
}

gboolean
rs_library_has_database_connection(RSLibrary *library)
{
	g_return_val_if_fail(RS_IS_LIBRARY(library), FALSE);

  if (library_execute_sql(library->db, "PRAGMA user_version;") == 0)
    return TRUE;
  else
    return FALSE;
}

gchar *
rs_library_get_init_error_msg(RSLibrary *library)
{
	g_return_val_if_fail(RS_IS_LIBRARY(library), NULL);

  return g_strdup(library->error_init);
}

static gint
library_set_version(sqlite3 *db, gint version)
{
	sqlite3_stmt *stmt;
	gint rc;

	rc = sqlite3_prepare_v2(db, "update version set version = ?1;", -1, &stmt, NULL);
	rc = sqlite3_bind_int(stmt, 1, version);
	rc = sqlite3_step(stmt);
	library_sqlite_error(db, rc);
	sqlite3_finalize(stmt);

	return SQLITE_OK;
}

static void
library_check_version(sqlite3 *db)
{
	sqlite3_stmt *stmt, *stmt_update;
	gint rc, version = 0, id;
	gchar *filename;

	rc = sqlite3_prepare_v2(db, "SELECT version FROM version", -1, &stmt, NULL);
	rc = sqlite3_step(stmt);
	if (rc == SQLITE_ROW)
		version = sqlite3_column_int(stmt, 0);
	rc = sqlite3_finalize(stmt);

	RS_DEBUG(LIBRARY, "Database version %d opened", version);

	while (version < LIBRARY_VERSION)
	{
		switch (version)
		{
		case 0:
			/* Alter table library - add identifier column */
			sqlite3_prepare_v2(db, "alter table library add column identifier varchar(32)", -1, &stmt, NULL);
			rc = sqlite3_step(stmt);
			library_sqlite_error(db, rc);
			sqlite3_finalize(stmt);

			/* Run through all photos in library and insert unique identifier in library */
			gchar *identifier;
			sqlite3_prepare_v2(db, "select filename from library", -1, &stmt, NULL);
			while (sqlite3_step(stmt) == SQLITE_ROW)
			{
				filename = (gchar *) sqlite3_column_text(stmt, 0);
				if (g_file_test(filename, G_FILE_TEST_EXISTS))
				{
					identifier = rs_file_checksum(filename);
					rc = sqlite3_prepare_v2(db, "update library set identifier = ?1 WHERE filename = ?2;", -1, &stmt_update, NULL);
					rc = sqlite3_bind_text(stmt_update, 1, identifier, -1, SQLITE_TRANSIENT);
					rc = sqlite3_bind_text(stmt_update, 2, filename, -1, SQLITE_TRANSIENT);
					rc = sqlite3_step(stmt_update);
					library_sqlite_error(db, rc);
					sqlite3_finalize(stmt_update);
					g_free(identifier);
				}
			}
			sqlite3_finalize(stmt);

			library_set_version(db, version+1);
			break;

		case 1:
			library_execute_sql(db, "BEGIN TRANSACTION;");
			sqlite3_prepare_v2(db, "select id,filename from library", -1, &stmt, NULL);
			while (sqlite3_step(stmt) == SQLITE_ROW)
			{
				id = (gint) sqlite3_column_int(stmt, 0);
				filename = rs_normalize_path((gchar *) sqlite3_column_text(stmt, 1));
				if (filename) /* FIXME: This will only work for paths that exists */
				{
					rc = sqlite3_prepare_v2(db, "update library set filename = ?1 WHERE id = ?2;", -1, &stmt_update, NULL);
					rc = sqlite3_bind_text(stmt_update, 1, filename, -1, SQLITE_TRANSIENT);
					rc = sqlite3_bind_int(stmt_update, 2, id);
					rc = sqlite3_step(stmt_update);
					library_sqlite_error(db, rc);
					sqlite3_finalize(stmt_update);
					g_free(filename);
				}
			}
			sqlite3_finalize(stmt);
			library_set_version(db, version+1);
			library_execute_sql(db, "COMMIT;");
			break;

		default:
			/* We should never hit this */
			g_warning("Some error occured in library_check_version() - please notify developers");
			break;
		}

		version++;
		RS_DEBUG(LIBRARY, "Updated library database to version %d", version);
	}
}

static void
rs_library_init(RSLibrary *library)
{
	int rc;

	gchar *database = g_strdup_printf("%s/.rawstudio/library.db", g_get_home_dir());

	/* If unable to create database we exit */
	if(sqlite3_open(database, &(library->db)))
	{
		gchar *msg = g_strdup_printf(_("Could not open database %s"), database);
		g_warning("sqlite3: %s\n", msg);
		if (library->error_init)
		  g_free(library->error_init);
		library->error_init = g_strdup(msg);
		sqlite3_close(library->db);
	}
	g_free(database);

	if (rs_library_has_database_connection(library))
	{
	  /* This is not FULL synchronous mode as default, since all data is re-creatable by local xml files.
	     From the sqlite3 manual:
	     With synchronous OFF (0), SQLite continues without syncing as soon as it has handed data off to 
	     the operating system. If the application running SQLite crashes, the data will be safe, but the 
	     database might become corrupted if the operating system crashes or the computer loses power before 
	     that data has been written to the disk surface. On the other hand, 
	     some operations are as much as 50 or more times faster with synchronous OFF. " */
	  library_execute_sql(library->db, "PRAGMA synchronous = OFF;");

	  /* Move our journal to memory, we're not doing banking for the Mafia */
	  library_execute_sql(library->db, "PRAGMA journal_mode = memory;");

	  /* Place temp tables in memory */
	  library_execute_sql(library->db, "PRAGMA temp_store = memory;");

	  rc = library_create_tables(library->db);
	  library_sqlite_error(library->db, rc);

	  library_check_version(library->db);

	  library->id_lock = g_mutex_new();
	}
}

RSLibrary *
rs_library_get_singleton(void)
{
	static GStaticMutex singleton_lock = G_STATIC_MUTEX_INIT;
	static RSLibrary *singleton = NULL;

	g_static_mutex_lock(&singleton_lock);
	if (!singleton)
		singleton = g_object_new(RS_TYPE_LIBRARY, NULL);
	g_static_mutex_unlock(&singleton_lock);

	return singleton;
}

static gint
library_execute_sql(sqlite3 *db, const gchar *sql)
{
	sqlite3_stmt *statement;

	if(SQLITE_OK != sqlite3_prepare(db, sql, -1, &statement, 0))
		return sqlite3_errcode(db);

	while (SQLITE_ROW == sqlite3_step(statement));

	return sqlite3_finalize(statement);
}

static void
library_sqlite_error(sqlite3 *db, gint result)
{
	if (result != SQLITE_OK && result != SQLITE_DONE)
	{
		g_warning("sqlite3 warning: %s\n", sqlite3_errmsg(db));
	}
}

static gint
library_create_tables(sqlite3 *db)
{
	sqlite3_stmt *stmt;
	gint rc;
	GTimer *gt = g_timer_new();
       
	/* Create table (library) to hold all known photos */
	sqlite3_prepare_v2(db, "create table library (id integer primary key, filename varchar(1024), identifier varchar(32))", -1, &stmt, NULL);
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

	/* Create table (version) to help keeping track of database version */
	sqlite3_prepare_v2(db, "create table version (version integer)", -1, &stmt, NULL);
	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	rc = sqlite3_prepare_v2(db, "select * from version", -1, &stmt, NULL);
	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	if (rc != SQLITE_ROW)
	{
		/* Set current version */
		rc = sqlite3_prepare_v2(db, "insert into version (version) values (?1);", -1, &stmt, NULL);
		rc = sqlite3_bind_int(stmt, 1, LIBRARY_VERSION);
		rc = sqlite3_step(stmt);
		sqlite3_finalize(stmt);

		rc = sqlite3_prepare_v2(db, "select identifier from library", -1, &stmt, NULL);
		rc = sqlite3_step(stmt);
		sqlite3_finalize(stmt);

		/* Check if library.identifier exists */
		if (rc == SQLITE_MISUSE)
		{
			library_set_version(db, 0);
		}
	}

	RS_DEBUG(LIBRARY, "Tables created in %.0fms", g_timer_elapsed(gt, NULL)*1000.0);

	g_timer_destroy(gt);

	return SQLITE_OK;
}

static gint
library_find_tag_id(RSLibrary *library, const gchar *tagname)
{
	sqlite3 *db = library->db;
	sqlite3_stmt *stmt;
	gint rc, tag_id = -1;

	rc = sqlite3_prepare_v2(db, "SELECT id FROM tags WHERE tagname = ?1;", -1, &stmt, NULL);
	rc = sqlite3_bind_text(stmt, 1, tagname, -1, SQLITE_TRANSIENT);
	rc = sqlite3_step(stmt);
	if (rc == SQLITE_ROW)
		tag_id = sqlite3_column_int(stmt, 0);
	rc = sqlite3_finalize(stmt);
	return tag_id;
}

static gint
library_find_photo_id(RSLibrary *library, const gchar *photo)
{
	sqlite3 *db = library->db;
	sqlite3_stmt *stmt;
	gint rc, photo_id = -1;

	rc = sqlite3_prepare_v2(db, "SELECT id FROM library WHERE filename = ?1;", -1, &stmt, NULL);
	rc = sqlite3_bind_text(stmt, 1, photo, -1, SQLITE_TRANSIENT);
	library_sqlite_error(db, rc);
	rc = sqlite3_step(stmt);
	if (rc == SQLITE_ROW)
		photo_id = sqlite3_column_int(stmt, 0);
	rc = sqlite3_finalize(stmt);
	return photo_id;
}

static void
library_photo_add_tag(RSLibrary *library, const gint photo_id, const gint tag_id, const gboolean autotag)
{
	sqlite3 *db = library->db;
	gint rc;
	sqlite3_stmt *stmt;

	gint autotag_tag = 0;
	if (autotag)
		autotag_tag = 1;

	g_mutex_lock(library->id_lock);
	rc = sqlite3_prepare_v2(db, "INSERT INTO phototags (photo, tag, autotag) VALUES (?1, ?2, ?3);", -1, &stmt, NULL);
	rc = sqlite3_bind_int (stmt, 1, photo_id);
	rc = sqlite3_bind_int (stmt, 2, tag_id);
	rc = sqlite3_bind_int (stmt, 3, autotag_tag);
	rc = sqlite3_step(stmt);
	g_mutex_unlock(library->id_lock);
	if (rc != SQLITE_DONE)
		library_sqlite_error(db, rc);
	sqlite3_finalize(stmt);
}

static gboolean
library_is_photo_tagged(RSLibrary *library, gint photo_id, gint tag_id)
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

static void
got_checksum(const gchar *checksum, gpointer user_data)
{
	RSLibrary *library = rs_library_get_singleton();
	sqlite3 *db = library->db;
	sqlite3_stmt *stmt;

	sqlite3_prepare_v2(db, "UPDATE LIBRARY SET  identifier=?1 WHERE id=?2;", -1, &stmt, NULL);
	sqlite3_bind_text(stmt, 1, checksum, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 2, GPOINTER_TO_INT(user_data));
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
}

static gint
library_add_photo(RSLibrary *library, const gchar *filename)
{
	gint id;
	sqlite3 *db = library->db;
	gint rc;
	sqlite3_stmt *stmt;

	g_mutex_lock(library->id_lock);
	sqlite3_prepare_v2(db, "INSERT INTO library (filename) VALUES (?1);", -1, &stmt, NULL);
	rc = sqlite3_bind_text(stmt, 1, filename, -1, SQLITE_TRANSIENT);
	rc = sqlite3_step(stmt);
	id = sqlite3_last_insert_rowid(db);
	g_mutex_unlock(library->id_lock);
	if (rc != SQLITE_DONE)
		library_sqlite_error(db, rc);
	sqlite3_finalize(stmt);

	rs_io_idle_read_checksum(filename, -1, got_checksum, GINT_TO_POINTER(id));

	return id;
}

static gint
library_add_tag(RSLibrary *library, const gchar *tagname)
{
	gint id;
	sqlite3 *db = library->db;
	gint rc;
	sqlite3_stmt *stmt;

	g_mutex_lock(library->id_lock);
	sqlite3_prepare_v2(db, "INSERT INTO tags (tagname) VALUES (?1);", -1, &stmt, NULL);
	rc = sqlite3_bind_text(stmt, 1, tagname, -1, SQLITE_TRANSIENT);
	rc = sqlite3_step(stmt);
	id = sqlite3_last_insert_rowid(db);
	g_mutex_unlock(library->id_lock);
	if (rc != SQLITE_DONE)
		library_sqlite_error(db, rc);
	sqlite3_finalize(stmt);

	return id;
}

static void 
library_delete_photo(RSLibrary *library, gint photo_id)
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

static void 
library_delete_tag(RSLibrary *library, gint tag_id)
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

static void 
library_photo_delete_tags(RSLibrary *library, gint photo_id)
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

static void
library_tag_delete_photos(RSLibrary *library, gint tag_id)
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

static gboolean
library_tag_is_used(RSLibrary *library, gint tag_id)
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

gint
rs_library_add_photo(RSLibrary *library, const gchar *filename)
{
	gint photo_id;

	g_return_val_if_fail(RS_IS_LIBRARY(library), 0);
	g_return_val_if_fail(filename != NULL, 0);

	if (!rs_library_has_database_connection(library)) return 0; /* FIXME */

	photo_id = library_find_photo_id(library, filename);
	if (photo_id == -1)
	{
		RS_DEBUG(LIBRARY, "'%s' added to library", filename);
		photo_id = library_add_photo(library, filename);
	}
	else
		RS_DEBUG(LIBRARY, "'%s' already found in library, not adding", filename);


	return photo_id;
}

gint
rs_library_add_tag(RSLibrary *library, const gchar *tagname)
{
	gint tag_id;

	g_return_val_if_fail(RS_IS_LIBRARY(library), 0);
	g_return_val_if_fail(tagname != NULL, 0);

	if (!rs_library_has_database_connection(library)) return 0; /* FIXME */

	tag_id = library_find_tag_id(library, tagname);
	if (tag_id == -1)
	{
		RS_DEBUG(LIBRARY, "Adding '%s' to tags-table",tagname);
		tag_id = library_add_tag(library, tagname);
	}

	return tag_id;
}

void
rs_library_photo_add_tag(RSLibrary *library, const gchar *filename, gint tag_id, const gboolean autotag)
{
	gint photo_id;

	g_return_if_fail(RS_IS_LIBRARY(library));

	if (!rs_library_has_database_connection(library)) return;

	if (tag_id == -1)
	{
		g_warning("Tag not known...");
		return;
	}

	photo_id = library_find_photo_id(library, filename);
	if (photo_id == -1)
	{
		g_warning("Photo not known...");
		return;
	}

	if (!library_is_photo_tagged(library, photo_id, tag_id))
		library_photo_add_tag(library, photo_id, tag_id, autotag);

	return;
}

void
rs_library_delete_photo(RSLibrary *library, const gchar *photo)
{
	g_return_if_fail(RS_IS_LIBRARY(library));

	if (!rs_library_has_database_connection(library)) return;

	gint photo_id = -1;

	photo_id = library_find_photo_id(library, photo);
	if (photo_id == -1)
	{
		g_warning("Photo not known...");
		return;
	}

	library_photo_delete_tags(library, photo_id);
	library_delete_photo(library, photo_id);
	rs_library_backup_tags(library, photo);
}

gboolean
rs_library_delete_tag(RSLibrary *library, const gchar *tag, const gboolean force)
{
	g_return_val_if_fail(RS_IS_LIBRARY(library), FALSE);
	g_return_val_if_fail(tag != NULL, FALSE);

	if (!rs_library_has_database_connection(library)) return FALSE;

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
rs_library_search(RSLibrary *library, const gchar *needle)
{
	g_return_val_if_fail(RS_IS_LIBRARY(library), NULL);
	g_return_val_if_fail(needle != NULL, NULL);

	if (!rs_library_has_database_connection(library)) return NULL;

	sqlite3_stmt *stmt;
	gint rc;
	sqlite3 *db = library->db;
	gchar *tag;
	gint n, num_tags;
	GList *photos = NULL;
	GTimer *gt = g_timer_new();
	gchar *filename;
	gchar **needle_parts;

	/* Create filter table */
	sqlite3_prepare_v2(db, "create temp table filter (photo integer)", -1, &stmt, NULL);
	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	library_sqlite_error(db, rc);

	needle_parts = g_strsplit_set(needle, " ", 0);
	num_tags = g_strv_length(needle_parts);

	/* Populate filter */
	for (n = 0; n < num_tags; n++)
	{
		tag = needle_parts[n];

		g_mutex_lock(library->id_lock);
		sqlite3_prepare_v2(db, "insert into filter select phototags.photo from phototags, tags where phototags.tag = tags.id and lower(tags.tagname) = lower(?1) ;", -1, &stmt, NULL);
		rc = sqlite3_bind_text(stmt, 1, tag, -1, SQLITE_TRANSIENT);
		rc = sqlite3_step(stmt);
		sqlite3_finalize(stmt);
		g_mutex_unlock(library->id_lock);
	}

	g_strfreev(needle_parts);

	RS_DEBUG(LIBRARY, "Filter table populated @%.0fms", g_timer_elapsed(gt, NULL)*1000.0);

	/* Create result table */
	sqlite3_prepare_v2(db, "create temp table result (photo integer, count integer)", -1, &stmt, NULL);
	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	library_sqlite_error(db, rc);

	/* Populate result table */
	g_mutex_lock(library->id_lock);
	sqlite3_prepare_v2(db, "insert into result select photo, count(photo) from filter group by photo;", -1, &stmt, NULL);
	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	g_mutex_unlock(library->id_lock);
	library_sqlite_error(db, rc);

	/* Get filename */
	sqlite3_prepare_v2(db, "select library.filename from library,result where library.id = result.photo and result.count = ?1 order by library.filename;", -1, &stmt, NULL);
        rc = sqlite3_bind_int(stmt, 1, num_tags);

	gint count = 0;
	while (sqlite3_step(stmt) == SQLITE_ROW && count < MAX_SEARCH_RESULTS)
	{
		filename = g_strdup((gchar *) sqlite3_column_text(stmt, 0));
		if (g_file_test(filename, G_FILE_TEST_EXISTS))
		{
			photos = g_list_append(photos, filename);
			count++;
		}
	}				       
	sqlite3_finalize(stmt);
	library_sqlite_error(db, rc);

	RS_DEBUG(LIBRARY, "Search done @%.0fms", g_timer_elapsed(gt, NULL)*1000.0);

	/* Empty filter */
	sqlite3_prepare_v2(db, "delete from filter;", -1, &stmt, NULL);
	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	library_sqlite_error(db, rc);

	/* Empty result */
	sqlite3_prepare_v2(db, "delete from result;", -1, &stmt, NULL);
	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	library_sqlite_error(db, rc);

	RS_DEBUG(LIBRARY, "Search for '%s' in library took %.0fms seconds", needle, g_timer_elapsed(gt, NULL)*1000.0);
	g_timer_destroy(gt);

	return photos;
}

static void
library_photo_default_tags(RSLibrary *library, const gint photo_id, RSMetadata *metadata)
{
	g_assert(RS_IS_LIBRARY(library));

	GList *tags = NULL;

	if (metadata->make_ascii)
	{
		GList *temp = rs_split_string(metadata->make_ascii, " ");
		tags = g_list_concat(tags, temp);
	}
	if (metadata->model_ascii)
	{
		GList *temp = rs_split_string(metadata->model_ascii, " ");
		tags = g_list_concat(tags, temp);
	}
	if (metadata->lens_min_focal != -1 && metadata->lens_max_focal != -1)
	{
		gchar *lens = NULL;
		if (metadata->lens_min_focal == metadata->lens_max_focal)
			lens = g_strdup_printf("%dmm",(gint) metadata->lens_min_focal);
		else
			lens = g_strdup_printf("%d-%dmm",(gint) metadata->lens_min_focal, (gint) metadata->lens_max_focal);
		tags = g_list_append(tags, g_strdup(lens));
		g_free(lens);
	}
	if (metadata->focallength > 0)
	{
		gchar *text = NULL;
		if (metadata->focallength < 50)
		  text = g_strdup(_("wideangle"));
		else
		  text = g_strdup(_("telephoto"));
		tags = g_list_append(tags, g_strdup(text));
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
			month = g_strdup(_("January")); /* FIXME: There may be a better way to do this */
			break;
		case 2:
			month = g_strdup(_("February"));
			break;
		case 3:
			month = g_strdup(_("March"));
			break;
		case 4:
			month = g_strdup(_("April"));
			break;
		case 5:
			month = g_strdup(_("May"));
			break;
		case 6:
			month = g_strdup(_("June"));
			break;
		case 7:
			month = g_strdup(_("July"));
			break;
		case 8:
			month = g_strdup(_("August"));
			break;
		case 9:
			month = g_strdup(_("September"));
			break;
		case 10:
			month = g_strdup(_("October"));
			break;
		case 11:
			month = g_strdup(_("November"));
			break;
		case 12:
			month = g_strdup(_("December"));
			break;
		}

		tags = g_list_append(tags, g_strdup(year));
		tags = g_list_append(tags, g_strdup(month));

		g_date_free(date);
		g_free(year);
		g_free(month);
	}

	gint i, j;
	library_execute_sql(library->db, "BEGIN TRANSACTION;");
	gint *used_tags = g_malloc(g_list_length(tags) * sizeof(gint));
	for(i = 0; i < g_list_length(tags); i++)
	{
		gchar *tag = (gchar *) g_list_nth_data(tags, i);
		gint tag_id = rs_library_add_tag(library, tag);

		/* Check if tag has already been added */
		gboolean used = FALSE;
		for (j = 0; j < i; j++)
			if (tag_id == used_tags[j])
				used = TRUE;

		if (!used)
			library_photo_add_tag(library, photo_id, tag_id, TRUE);
		used_tags[i] = tag_id;
		g_free(tag);
	}
	g_free(used_tags);
	library_execute_sql(library->db, "COMMIT;");
	g_list_free(tags);
}

GList *
rs_library_photo_tags(RSLibrary *library, const gchar *photo, const gboolean autotag)
{
	g_return_val_if_fail(RS_IS_LIBRARY(library), NULL);
	g_return_val_if_fail(photo != NULL, NULL);

	if (!rs_library_has_database_connection(library)) return NULL;

	sqlite3_stmt *stmt;
	gint rc;
	sqlite3 *db = library->db;
	GList *tags = NULL;

	if (autotag)
	{
		sqlite3_prepare_v2(db, "select tags.tagname from library,phototags,tags WHERE library.id=phototags.photo and phototags.tag=tags.id and library.filename = ?1;", -1, &stmt, NULL);
		rc = sqlite3_bind_text(stmt, 1, photo, -1, NULL);
	}
	else
	{
		sqlite3_prepare_v2(db, "select tags.tagname from library,phototags,tags WHERE library.id=phototags.photo and phototags.tag=tags.id and library.filename = ?1 and phototags.autotag = 0;", -1, &stmt, NULL);
		rc = sqlite3_bind_text(stmt, 1, photo, -1, NULL);
	}
	while (sqlite3_step(stmt) == SQLITE_ROW)
		tags = g_list_append(tags, g_strdup((gchar *) sqlite3_column_text(stmt, 0)));
	sqlite3_finalize(stmt);
	library_sqlite_error(db, rc);

	return tags;
}

GList *
rs_library_find_tag(RSLibrary *library, const gchar *tag)
{
	g_return_val_if_fail(RS_IS_LIBRARY(library), NULL);
	g_return_val_if_fail(tag != NULL, NULL);

	if (!rs_library_has_database_connection(library)) return NULL;

	sqlite3_stmt *stmt;
	gint rc;
	sqlite3 *db = library->db;
	GList *tags = NULL;

	rc = sqlite3_prepare_v2(db, "select tags.tagname from tags WHERE tags.tagname like ?1 order by tags.tagname;", -1, &stmt, NULL);
	gchar *like = g_strdup_printf("%%%s%%", tag);
        rc = sqlite3_bind_text(stmt, 1, like, -1, NULL);
	library_sqlite_error(db, rc);
	
	while (sqlite3_step(stmt) == SQLITE_ROW)
		tags = g_list_append(tags, g_strdup((gchar *) sqlite3_column_text(stmt, 0)));
	sqlite3_finalize(stmt);
	library_sqlite_error(db, rc);

	g_free(like);

	return tags;
}


gboolean
rs_library_set_tag_search(gchar *str)
{
	if (!str)
		return FALSE;
	gtk_entry_set_text(GTK_ENTRY(tag_search_entry), str);
	return TRUE;
}

void
rs_library_add_photo_with_metadata(RSLibrary *library, const gchar *photo, RSMetadata *metadata)
{
	g_return_if_fail(RS_IS_LIBRARY(library));
	g_return_if_fail(photo != NULL);
	g_return_if_fail(RS_IS_METADATA(metadata));

	if (!rs_library_has_database_connection(library)) return;

	RS_DEBUG(LIBRARY, "Adding '%s' to library", photo);

	/* Bail out if we already know the photo */
	if (library_find_photo_id(library, photo) > -1)
		return;

	gint photo_id = library_add_photo(library, photo);
	library_photo_default_tags(library, photo_id, metadata);
}

static GStaticMutex backup_lock = G_STATIC_MUTEX_INIT;

void 
rs_library_backup_tags(RSLibrary *library, const gchar *photo_filename)
{
	g_return_if_fail(RS_IS_LIBRARY(library));
	g_return_if_fail(photo_filename != NULL);

	RS_DEBUG(LIBRARY, "Backing up tags for '%s'", photo_filename);

	if (!rs_library_has_database_connection(library)) return;

	GTimer *gt;
	sqlite3 *db = library->db;
	sqlite3_stmt *stmt;
	gint rc;
	gchar *filename = NULL, *checksum, *tag, *t_filename;
	gint autotag;
	gchar *directory = g_path_get_dirname(photo_filename);
	gchar *dotdir = rs_dotdir_get(photo_filename);

	g_static_mutex_lock (&backup_lock);

	if (!dotdir)
		return;

	gt = g_timer_new();

	GString *gs = g_string_new(dotdir);
	g_string_append(gs, G_DIR_SEPARATOR_S);
	g_string_append(gs, TAGS_XML_FILE);
	gchar *xmlfile = gs->str;
	g_string_free(gs, FALSE);

	xmlTextWriterPtr writer;

	writer = xmlNewTextWriterFilename(xmlfile, 0);
	if (!writer)
	{
		g_timer_destroy(gt);
		g_free(directory);
		g_free(dotdir);
		g_free(xmlfile);
		g_static_mutex_unlock (&backup_lock);	
		return;
	}

	xmlTextWriterSetIndent(writer, 1);
	xmlTextWriterStartDocument(writer, NULL, "UTF-8", NULL);
	xmlTextWriterStartElement(writer, BAD_CAST "rawstudio-tags");
	xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "version", "%d", LIBRARY_VERSION);

	const gchar *temp = g_strdup_printf("%s/%%", directory);
	rc = sqlite3_prepare_v2(db, "select library.filename,library.identifier,tags.tagname,phototags.autotag from library,phototags,tags where library.filename like ?1 and phototags.photo = library.id and tags.id = phototags.tag order by library.filename;", -1, &stmt, NULL);
	rc = sqlite3_bind_text(stmt, 1, temp, -1, SQLITE_TRANSIENT);
	library_sqlite_error(db, rc);
	while (sqlite3_step(stmt) == SQLITE_ROW)
	{
		t_filename = g_path_get_basename((gchar *) sqlite3_column_text(stmt, 0));
		if (g_strcmp0(t_filename, filename) != 0 || filename == NULL)
		{
			if (filename != NULL)
				xmlTextWriterEndElement(writer);
			filename = t_filename;
			xmlTextWriterStartElement(writer, BAD_CAST "file");
			xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "name", "%s", filename);
			checksum = (gchar *) sqlite3_column_text(stmt, 1);
			xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "checksum", "%s", checksum);
		}

		tag = (gchar *) sqlite3_column_text(stmt, 2);
		autotag = (gint) sqlite3_column_int(stmt, 3);
		xmlTextWriterStartElement(writer, BAD_CAST "tag");
		xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "name", "%s", tag);
		xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "auto", "%d", autotag);
		xmlTextWriterEndElement(writer);
	}
	xmlTextWriterEndElement(writer);

	rc = sqlite3_finalize(stmt);

	xmlTextWriterEndDocument(writer);
	xmlFreeTextWriter(writer);
	g_free(directory);
	g_free(dotdir);
	g_free(xmlfile);
	g_static_mutex_unlock (&backup_lock);	

	RS_DEBUG(PERFORMANCE, "Backup done in %.0fms", g_timer_elapsed(gt, NULL)*1000.0);
	g_timer_destroy(gt);

	return;
}

void 
rs_library_restore_tags(const gchar *directory)
{
	GTimer *gt;
	g_return_if_fail(directory != NULL);

	RS_DEBUG(LIBRARY, "Restoring tags from '%s'", directory);

	RSLibrary *library = rs_library_get_singleton();

	if (!rs_library_has_database_connection(library)) return;

	gchar *dotdir = rs_dotdir_get(directory);

	gt = g_timer_new();

	if (!dotdir)
		return;
	GString *gs = g_string_new(dotdir);
	g_string_append(gs, G_DIR_SEPARATOR_S);
	g_string_append(gs, TAGS_XML_FILE);
	gchar *xmlfile = gs->str;
	g_string_free(gs, FALSE);

	if (!g_file_test(xmlfile, G_FILE_TEST_EXISTS))
	{
		g_timer_destroy(gt);
		g_free(dotdir);
		g_free(xmlfile);
		return;
	}

	xmlDocPtr doc;
	xmlNodePtr cur, cur2;
	xmlChar *val;
	gint version = 0;

	gchar *filename, *identifier, *tagname;
	gint autotag, photoid, tagid;

	doc = xmlParseFile(xmlfile);
	if (!doc)
		return;

	cur = xmlDocGetRootElement(doc);

	if ((!xmlStrcmp(cur->name, BAD_CAST "rawstudio-tags")))
	{
		val = xmlGetProp(cur, BAD_CAST "version");
		if (val)
			version = atoi((gchar *) val);
		if (version > LIBRARY_VERSION)
		{
			xmlFree(val);
			g_free(dotdir);
			g_free(xmlfile);
			xmlFreeDoc(doc);
			return;
		}
	}

	cur = cur->xmlChildrenNode;
	while(cur)
	{
		if ((!xmlStrcmp(cur->name, BAD_CAST "file")))
		{
			val = xmlGetProp(cur, BAD_CAST "name");
			filename = g_build_filename(directory, val, NULL);
			xmlFree(val);

			photoid = library_find_photo_id(library, filename);
			if ( photoid == -1 && g_file_test(filename, G_FILE_TEST_EXISTS))
			{
				photoid = rs_library_add_photo(library, filename);

				val = xmlGetProp(cur, BAD_CAST "checksum");
				identifier = (gchar *) val;

				cur2 = cur->xmlChildrenNode;
				while(cur2)
				{
					if ((!xmlStrcmp(cur2->name, BAD_CAST "tag")))
					{
						val = xmlGetProp(cur2, BAD_CAST "name");
						tagname =(gchar*) val;
						tagid = library_find_tag_id(library, tagname);
						if ( tagid == -1)
							tagid = rs_library_add_tag(library, tagname);

						val = xmlGetProp(cur2, BAD_CAST "auto");
						autotag = atoi((gchar *) val);
						xmlFree(val);

						library_photo_add_tag(library, photoid, tagid, (autotag == 1));

						xmlFree(tagname);
					}
					cur2 = cur2->next;
				}
				xmlFree(identifier);
			}
			g_free(filename);
		}
		cur = cur->next;
	}

	g_free(dotdir);
	g_free(xmlfile);
	xmlFreeDoc(doc);

	RS_DEBUG(PERFORMANCE, "Restored in %.0fms", g_timer_elapsed(gt, NULL)*1000.0);
	g_timer_destroy(gt);
	return;
}
