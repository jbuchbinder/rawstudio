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

#include <rawstudio.h>
#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include <config.h>
#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
#include <lensfun.h>
#include <gettext.h>
#include <rs-lens.h>
#include "rs-lensfun-select.h"

void lensfun_editor_load_database(GtkTreeModel *list);
GtkWidget *gui_dialog_make_from_widget(const gchar *stock_id, gchar *primary_text, GtkWidget *widget); /* FIXME */

/* BEGIN BLOCK - mostly grabbed from UFRaw */
typedef struct {
	/* The GtkEntry with camera maker/model name */
	GtkWidget *CameraModel;
	/* The menu used to choose camera - either full or limited by search criteria */
	GtkWidget *CameraMenu;
	/* The GtkEntry with lens maker/model name */
	GtkWidget *LensModel;
	/* The menu used to choose lens - either full or limited by search criteria */
	GtkWidget *LensMenu;
	/* */
	RSLens *lens;
} lens_data;

static void lens_set (lens_data *data, const lfLens *lens)
{
	g_print ("Lens: %s / %s\n",
		 lf_mlstr_get (lens->Maker),
		 lf_mlstr_get (lens->Model));
        g_print ("\tCrop factor: %g\n", lens->CropFactor);
        g_print ("\tFocal: %g-%g\n", lens->MinFocal, lens->MaxFocal);
        g_print ("\tAperture: %g-%g\n", lens->MinAperture, lens->MaxAperture);
        g_print ("\tCenter: %g,%g\n", lens->CenterX, lens->CenterY);
        g_print ("\tCCI: %g/%g/%g\n", lens->RedCCI, lens->GreenCCI, lens->BlueCCI);

	RSLens *rslens = data->lens;

	rs_lens_set_lensfun_make(rslens, g_strdup(lens->Maker));
	rs_lens_set_lensfun_model(rslens, g_strdup(lens->Model));
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
//		gtk_widget_destroy (data->LensMenu);
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


void lens_clicked (GtkButton *button, gpointer user_data)
{
	struct lfDatabase *lensdb = NULL;
	lfCamera *camera = NULL;

	lens_data *data = g_malloc(sizeof(lens_data));

	data->lens = (RSLens *) user_data;

	lensdb = lf_db_new ();
	lf_db_load (lensdb);

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
/* END BLOCK - mostly grabbed from UFRaw */

GtkWidget *
rs_lensfun_select_lens(RSLens *lens)
{
	GtkWidget *box = gtk_hbox_new(TRUE, 2);

	GtkWidget *name = gtk_label_new("Camera: Canon EOS 40D\nLens Aperture: f/1.4-32\nLens focallength: 50mm");
	GtkWidget *button = gtk_button_new_from_stock(GTK_STOCK_FIND);

        g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (lens_clicked), lens);

	gtk_box_pack_start (GTK_BOX (box), name, TRUE, TRUE, 0);
	gtk_box_pack_end (GTK_BOX (box), button, FALSE, FALSE, 0);
	GtkWidget *dialog = gui_dialog_make_from_widget(GTK_STOCK_FIND, _("Unknown lens, please select the correct one..."), box);
	gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT);
	gtk_widget_show_all(dialog);

	return dialog;
}
