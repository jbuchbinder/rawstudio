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

/*
 * The following functions is more or less grabbed from UFraw:
 * lens_set(), lens_menu_select(), ptr_array_insert_sorted(),
 * ptr_array_find_sorted(), ptr_array_insert_index() and lens_menu_fill()
 */

#include <rawstudio.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <string.h>
#include <config.h>
#include <lensfun.h>
#include <rs-lens-db.h>
#include <rs-lens.h>
#include <gettext.h>
#include "rs-lens-db-editor.h"

static void fill_model(RSLensDb *lens_db, GtkTreeModel *tree_model);

typedef struct {
	/* The menu used to choose lens - either full or limited by search criteria */
	GtkWidget *LensMenu;
	/* The GtkTreeView */
	GtkTreeView *tree_view;
} lens_data;

static void lens_set (lens_data *data, const lfLens *lens)
{
	GtkTreeSelection *selection = gtk_tree_view_get_selection(data->tree_view);
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;

	gtk_tree_selection_get_selected(selection, &model, &iter);

	/* Set Maker and Model to the tree view */
	gtk_list_store_set (GTK_LIST_STORE(model), &iter,
			    RS_LENS_DB_EDITOR_LENS_MAKE, lens->Maker,
			    RS_LENS_DB_EDITOR_LENS_MODEL, lens->Model,
			    -1);

	RSLens *rs_lens = NULL;
	gtk_tree_model_get (model, &iter,
			    RS_LENS_DB_EDITOR_LENS, &rs_lens,
			    -1);

	/* Set Maker and Model to the selected RSLens */
	rs_lens_set_lensfun_make(rs_lens, lens->Maker);
	rs_lens_set_lensfun_model(rs_lens, lens->Model);

	RSLensDb *lens_db = rs_lens_db_get_default();

	/* Force save of RSLensDb */
	rs_lens_db_save(lens_db);
}


static void lens_menu_select (
	GtkMenuItem *menuitem, gpointer user_data)
{
	lens_data *data = (lens_data *)user_data;
	lens_set (data, (lfLens *)g_object_get_data(G_OBJECT(menuitem), "lfLens"));
}

int ptr_array_insert_sorted (
	GPtrArray *array, const void *item, GCompareFunc compare)
{
	int length = array->len;
	g_ptr_array_set_size (array, length + 1);
	const void **root = (const void **)array->pdata;

	int m = 0, l = 0, r = length - 1;

	// Skip trailing NULL, if any
	if (l <= r && !root [r])
		r--;
    
	while (l <= r)
	{
		m = (l + r) / 2;
		int cmp = compare (root [m], item);

		if (cmp == 0)
		{
			++m;
			goto done;
		}
		else if (cmp < 0)
			l = m + 1;
		else
			r = m - 1;
	}
	if (r == m)
		m++;

  done:
	memmove (root + m + 1, root + m, (length - m) * sizeof (void *));
	root [m] = item;
	return m;
}

int ptr_array_find_sorted (
	const GPtrArray *array, const void *item, GCompareFunc compare)
{
	int length = array->len;
	void **root = array->pdata;

	int l = 0, r = length - 1;
	int m = 0, cmp = 0;

	if (!length)
		return -1;

	// Skip trailing NULL, if any
	if (!root [r])
		r--;

	while (l <= r)
	{
		m = (l + r) / 2;
		cmp = compare (root [m], item);

		if (cmp == 0)
			return m;
		else if (cmp < 0)
			l = m + 1;
		else
			r = m - 1;
	}
    
	return -1;
}


void ptr_array_insert_index (
	GPtrArray *array, const void *item, int index)
{
	const void **root;
	int length = array->len;
	g_ptr_array_set_size (array, length + 1);
	root = (const void **)array->pdata;
	memmove (root + index + 1, root + index, (length - index) * sizeof (void *));
	root [index] = item;
}



static void lens_menu_fill (
	lens_data *data, const lfLens *const *lenslist)
{
	unsigned i;
	GPtrArray *makers, *submenus;

	if (data->LensMenu)
	{
		/* This doesn't work, but will we be leaking GtkMenu's */
		//gtk_widget_destroy (data->LensMenu);
		data->LensMenu = NULL;
	}

	/* Count all existing lens makers and create a sorted list */
	makers = g_ptr_array_new ();
	submenus = g_ptr_array_new ();
	for (i = 0; lenslist [i]; i++)
	{
		GtkWidget *submenu, *item;
		const char *m = lf_mlstr_get (lenslist [i]->Maker);
		int idx = ptr_array_find_sorted (makers, m, (GCompareFunc)g_utf8_collate);
		if (idx < 0)
		{
			/* No such maker yet, insert it into the array */
			idx = ptr_array_insert_sorted (makers, m, (GCompareFunc)g_utf8_collate);
			/* Create a submenu for lenses by this maker */
			submenu = gtk_menu_new ();
			ptr_array_insert_index (submenus, submenu, idx);
		}
		submenu = g_ptr_array_index (submenus, idx);
		/* Append current lens name to the submenu */
		item = gtk_menu_item_new_with_label (lf_mlstr_get (lenslist [i]->Model));
		gtk_widget_show (item);
		g_object_set_data(G_OBJECT(item), "lfLens", (void *)lenslist [i]);
		g_signal_connect(G_OBJECT(item), "activate",
				 G_CALLBACK(lens_menu_select), data);
		gtk_menu_shell_append (GTK_MENU_SHELL (submenu), item);
	}

	data->LensMenu = gtk_menu_new ();
	for (i = 0; i < makers->len; i++)
	{
		GtkWidget *item = gtk_menu_item_new_with_label (g_ptr_array_index (makers, i));
		gtk_widget_show (item);
		gtk_menu_shell_append (GTK_MENU_SHELL (data->LensMenu), item);
		gtk_menu_item_set_submenu (
			GTK_MENU_ITEM (item), (GtkWidget *)g_ptr_array_index (submenus, i));
	}

	g_ptr_array_free (submenus, TRUE);
	g_ptr_array_free (makers, TRUE);
}

void row_clicked (GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data)
{
	struct lfDatabase *lensdb = NULL;
	const lfCamera *camera = NULL;
	const lfCamera **cameras = NULL;

	lens_data *data = g_malloc(sizeof(lens_data));
	data->tree_view = tree_view;

	lensdb = lf_db_new ();
	lf_db_load (lensdb);

	GtkTreeSelection *selection = gtk_tree_view_get_selection(data->tree_view);
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;

	gtk_tree_selection_get_selected(selection, &model, &iter);

	RSLens *rs_lens = NULL;
	gtk_tree_model_get (model, &iter,
			    RS_LENS_DB_EDITOR_LENS, &rs_lens,
			    -1);

	gchar *camera_make;
	gchar *camera_model;

	g_assert(RS_IS_LENS(rs_lens));
	g_object_get(rs_lens,
		     "camera-make", &camera_make,
		     "camera-model", &camera_model,
		     NULL);

	cameras = lf_db_find_cameras(lensdb, camera_make, camera_model);
	if (cameras)
		camera = cameras[0];

	if (camera)
	{
		const lfLens **lenslist = lf_db_find_lenses_hd (
			lensdb, camera, NULL, NULL, 0);

		if (!lenslist)
			return;
		lens_menu_fill (data, lenslist);
		lf_free (lenslist);
	}
	else
	{
		const lfLens *const *lenslist = lf_db_get_lenses (lensdb);

		if (!lenslist)
			return;
		lens_menu_fill (data, lenslist);
	}

	gtk_menu_popup (GTK_MENU (data->LensMenu), NULL, NULL, NULL, NULL,
			0, gtk_get_current_event_time ());
}

toggle_clicked (GtkCellRendererToggle *cell_renderer_toggle, const gchar *path, gpointer user_data)
{
	GtkTreeIter iter;
	gboolean enabled;
	GtkTreeView *tree_view = GTK_TREE_VIEW(user_data);
	GtkTreeModel *tree_model = gtk_tree_view_get_model(tree_view);
	GtkTreePath* tree_path = gtk_tree_path_new_from_string(path);

	gtk_tree_model_get_iter(GTK_TREE_MODEL (tree_model), &iter, tree_path);
	gtk_tree_model_get(GTK_TREE_MODEL (tree_model), &iter, RS_LENS_DB_EDITOR_ENABLED, &enabled, -1);

	if (enabled)
		gtk_list_store_set(GTK_LIST_STORE (tree_model), &iter, RS_LENS_DB_EDITOR_ENABLED, FALSE, -1);
	else
		gtk_list_store_set(GTK_LIST_STORE (tree_model), &iter, RS_LENS_DB_EDITOR_ENABLED, TRUE, -1);

	RSLens *rs_lens = NULL;
	gtk_tree_model_get (tree_model, &iter,
			    RS_LENS_DB_EDITOR_LENS, &rs_lens,
			    -1);

	/* Set enabled/disabled to the selected RSLens */
	rs_lens_set_lensfun_enabled(rs_lens, !enabled);

	RSLensDb *lens_db = rs_lens_db_get_default();

	/* Force save of RSLensDb */
	rs_lens_db_save(lens_db);
}

void
rs_lens_db_editor() 
{
	GtkTreeModel *tree_model = GTK_TREE_MODEL(gtk_list_store_new(9, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_OBJECT));

	RSLensDb *lens_db = rs_lens_db_get_default();
	fill_model(lens_db, tree_model);

	GtkWidget *editor = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(editor), "Rawstudio Lens Editor");

        GtkWidget *scroller = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroller),
					GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        GtkWidget *view = gtk_tree_view_new_with_model(tree_model);

        gtk_tree_view_set_reorderable(GTK_TREE_VIEW(view), FALSE);
        gtk_container_add (GTK_CONTAINER (scroller), view);

        GtkCellRenderer *renderer_lens_make = gtk_cell_renderer_text_new();
        GtkCellRenderer *renderer_lens_model = gtk_cell_renderer_text_new();
        GtkCellRenderer *renderer_focal = gtk_cell_renderer_text_new();
        GtkCellRenderer *renderer_aperture = gtk_cell_renderer_text_new();
        GtkCellRenderer *renderer_camera_make = gtk_cell_renderer_text_new();
        GtkCellRenderer *renderer_camera_model = gtk_cell_renderer_text_new();
        GtkCellRenderer *renderer_enabled = gtk_cell_renderer_toggle_new();

        GtkTreeViewColumn *column_lens_make = gtk_tree_view_column_new_with_attributes (_("Lens make"),
								  renderer_lens_make,
								  "text", RS_LENS_DB_EDITOR_LENS_MAKE,
										   NULL);
        GtkTreeViewColumn *column_lens_model = gtk_tree_view_column_new_with_attributes (_("Lens model"),
								  renderer_lens_model,
								  "text", RS_LENS_DB_EDITOR_LENS_MODEL,
										   NULL);
        GtkTreeViewColumn *column_focal = gtk_tree_view_column_new_with_attributes (_("Focal"),
								  renderer_focal,
								  "text", RS_LENS_DB_EDITOR_HUMAN_FOCAL,
										   NULL);
        GtkTreeViewColumn *column_aperture = gtk_tree_view_column_new_with_attributes (_("Aperture"),
								  renderer_aperture,
								  "text", RS_LENS_DB_EDITOR_HUMAN_APERTURE,
										   NULL);
        GtkTreeViewColumn *column_camera_make = gtk_tree_view_column_new_with_attributes (_("Camera make"),
								  renderer_camera_make,
								  "text", RS_LENS_DB_EDITOR_CAMERA_MAKE,
										   NULL);
        GtkTreeViewColumn *column_camera_model = gtk_tree_view_column_new_with_attributes (_("Camera model"),
								  renderer_camera_model,
								  "text", RS_LENS_DB_EDITOR_CAMERA_MODEL,
										   NULL);
        GtkTreeViewColumn *column_enabled = gtk_tree_view_column_new_with_attributes (_("Enabled"),
								  renderer_enabled,
								  "active", RS_LENS_DB_EDITOR_ENABLED,
										   NULL);

	g_signal_connect(G_OBJECT(view), "row-activated",
			 G_CALLBACK(row_clicked), NULL);

        g_signal_connect (renderer_enabled, "toggled",
			  G_CALLBACK (toggle_clicked), view);

        gtk_tree_view_append_column (GTK_TREE_VIEW (view), column_lens_make);
        gtk_tree_view_append_column (GTK_TREE_VIEW (view), column_lens_model);
        gtk_tree_view_append_column (GTK_TREE_VIEW (view), column_focal);
        gtk_tree_view_append_column (GTK_TREE_VIEW (view), column_aperture);
        gtk_tree_view_append_column (GTK_TREE_VIEW (view), column_camera_make);
        gtk_tree_view_append_column (GTK_TREE_VIEW (view), column_camera_model);
        gtk_tree_view_append_column (GTK_TREE_VIEW (view), column_enabled);

        gtk_tree_view_set_headers_visible(GTK_TREE_VIEW (view), TRUE);

        gtk_container_add (GTK_CONTAINER (editor), scroller);

	gtk_window_resize(GTK_WINDOW(editor), 400, 400);
	gtk_window_set_position(GTK_WINDOW(editor), GTK_WIN_POS_CENTER);
	gtk_widget_show_all(editor);
}

static void
fill_model(RSLensDb *lens_db, GtkTreeModel *tree_model)
{
	GList *list = rs_lens_db_get_lenses(lens_db);

        while (list)
        {
		gchar *identifier;
                gchar *lensfun_make;
                gchar *lensfun_model;
                gdouble min_focal, max_focal, min_aperture, max_aperture;
                gchar *camera_make;
                gchar *camera_model;
		gboolean enabled;

                RSLens *lens = list->data;

                g_assert(RS_IS_LENS(lens));
                g_object_get(lens,
			     "identifier", &identifier,
			     "lensfun-make", &lensfun_make,
			     "lensfun-model", &lensfun_model,
			     "min-focal", &min_focal,
			     "max-focal", &max_focal,
			     "min-aperture", &min_aperture,
			     "max-aperture", &max_aperture,
			     "camera-make", &camera_make,
			     "camera-model", &camera_model,
			     "enabled", &enabled,
			     NULL);

		const gchar *human_focal = rs_human_focal(min_focal, max_focal);
		const gchar *human_aperture = rs_human_aperture(max_aperture);

		GtkTreeIter iter;

		gtk_list_store_append (GTK_LIST_STORE(tree_model), &iter);
		gtk_list_store_set (GTK_LIST_STORE(tree_model), &iter,
				    RS_LENS_DB_EDITOR_IDENTIFIER, identifier,
				    RS_LENS_DB_EDITOR_HUMAN_FOCAL, human_focal,
				    RS_LENS_DB_EDITOR_HUMAN_APERTURE, human_aperture,
				    RS_LENS_DB_EDITOR_LENS_MAKE, lensfun_make,
				    RS_LENS_DB_EDITOR_LENS_MODEL, lensfun_model,
				    RS_LENS_DB_EDITOR_CAMERA_MAKE, camera_make,
				    RS_LENS_DB_EDITOR_CAMERA_MODEL, camera_model,
				    RS_LENS_DB_EDITOR_ENABLED, enabled,
				    RS_LENS_DB_EDITOR_LENS, lens,
				    -1);
		list = g_list_next (list);
	}
}
