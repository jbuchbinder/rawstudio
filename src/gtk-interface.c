#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include "dcraw_api.h"
#include "color.h"
#include "matrix.h"
#include "rawstudio.h"
#include "gtk-interface.h"
#include "conf_interface.h"
#include <string.h>
#include <unistd.h>

GtkStatusbar *statusbar;

void
gui_status_push(const char *text)
{
	gtk_statusbar_push(statusbar, gtk_statusbar_get_context_id(statusbar, "generic"), text);
	return;
}

void
gui_status_pop()
{
	gtk_statusbar_pop(statusbar, gtk_statusbar_get_context_id(statusbar, "generic"));
	return;
}

gboolean
update_preview_callback(GtkAdjustment *caller, RS_BLOB *rs)
{
	update_preview(rs);
	return(FALSE);
}

void update_histogram(RS_BLOB *rs)
{
	guint c,i,x,y,rowstride;
	guint max = 0;
	guint factor = 0;
	guint hist[3][rs->histogram_w];
	GdkPixbuf *pixbuf;
	guchar *pixels, *p;

	pixbuf = gtk_image_get_pixbuf(rs->histogram_image);
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  	pixels = gdk_pixbuf_get_pixels (pixbuf);
  	
	// sets all the pixels black
	memset(pixels, 0x00, rowstride*rs->histogram_h);
	
	// draw a grid with 7 bars with 32 pixels space
	p = pixels;
	for(y = 0; y < rs->histogram_h; y++)
	{
		for(x = 0; x < rs->histogram_w * 3; x +=93)
		{
			p[x++] = 100;
			p[x++] = 100;
			p[x++] = 100;
		}
		p+=rowstride;
	}
	
	// find the max value
	for (c = 0; c < 3; c++)
	{
		for (i = 0; i < rs->histogram_w; i++)
		{
			_MAX(rs->histogram_table[c][i], max);
		}
	}
	
	// find the factor to scale the histogram
	factor = (max+rs->histogram_h)/rs->histogram_h;

	// calculate the histogram values
	for (c = 0; c < 3; c++)
	{
		for (i = 0; i < rs->histogram_w; i++)
		{
			hist[c][i] = rs->histogram_table[c][i]/factor;
		}
	}
			
	// draw the histogram
	for (x = 0; x < rs->histogram_w; x++)
	{
		for (c = 0; c < 3; c++)
		{
			for (y = 0; y < hist[c][x]; y++)
			{				
				// address the pixel - the (rs->hist_h-1)-y is to draw it from the bottom
				p = pixels + ((rs->histogram_h-1)-y) * rowstride + x * 3;
				p[c] = 0xFF;
			}
		}
	}	
	gtk_image_set_from_pixbuf((GtkImage *) rs->histogram_image, pixbuf);

}

GtkObject *
make_adj(RS_BLOB *rs, double value, double min, double max, double step, double page)
{
	GtkObject *adj;
	adj = gtk_adjustment_new(value, min, max, step, page, 0.0);
	gtk_signal_connect(GTK_OBJECT(adj), "value_changed",
		G_CALLBACK(update_preview_callback), rs);
	return(adj);
}

GtkWidget *
gui_hist(RS_BLOB *rs, const gchar *label)
{
	GdkPixbuf *pixbuf;

	rs->histogram_w = 256;
	rs->histogram_h = 128;

	guint rowstride;
	guchar *pixels;

	// creates the pixbuf containing the histogram 
	pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, rs->histogram_w, rs->histogram_h);
	
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  	pixels = gdk_pixbuf_get_pixels (pixbuf);

	// sets all the pixels black
	memset(pixels, 0x00, rowstride*rs->histogram_h);
	
	// creates an image from the histogram pixbuf 
	rs->histogram_image = (GtkImage *) gtk_image_new_from_pixbuf(pixbuf);
	
	return(gui_box(label, (GtkWidget *)rs->histogram_image));
}

GtkWidget *
gui_box(const gchar *title, GtkWidget *in)
{
	GtkWidget *expander, *label;
	
	expander = gtk_expander_new (NULL);
	gtk_widget_show (expander);
	gtk_expander_set_expanded (GTK_EXPANDER (expander), TRUE);

	label = gtk_label_new (title);
	gtk_widget_show (label);
	gtk_expander_set_label_widget (GTK_EXPANDER (expander), label);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_container_add (GTK_CONTAINER (expander), in);
	return(expander);
}

void
gui_transform_rot90_clicked(GtkWidget *w, RS_BLOB *rs)
{
	DIRECTION_90(rs->direction);
	update_preview(rs);
}

void
gui_transform_rot180_clicked(GtkWidget *w, RS_BLOB *rs)
{
	DIRECTION_180(rs->direction);
	update_preview(rs);
}

void
gui_transform_rot270_clicked(GtkWidget *w, RS_BLOB *rs)
{
	DIRECTION_270(rs->direction);
	update_preview(rs);
}

void
gui_transform_mirror_clicked(GtkWidget *w, RS_BLOB *rs)
{
	DIRECTION_MIRROR(rs->direction);
	update_preview(rs);
}

void
gui_transform_flip_clicked(GtkWidget *w, RS_BLOB *rs)
{
	DIRECTION_FLIP(rs->direction);
	update_preview(rs);
}

GtkWidget *
gui_transform(RS_BLOB *rs)
{
	GtkWidget *hbox;
	GtkWidget *flip;
	GtkWidget *mirror;
	GtkWidget *rot90;
	GtkWidget *rot180;
	GtkWidget *rot270;
	
	hbox = gtk_hbox_new(TRUE, 0);
	flip = gtk_button_new_with_mnemonic ("Flip");
	mirror = gtk_button_new_with_mnemonic ("Mirror");
	rot90 = gtk_button_new_with_mnemonic ("CW");
	rot180 = gtk_button_new_with_mnemonic ("180");
	rot270 = gtk_button_new_with_mnemonic ("CCW");
	g_signal_connect ((gpointer) flip, "clicked", G_CALLBACK (gui_transform_flip_clicked), rs);
	g_signal_connect ((gpointer) mirror, "clicked", G_CALLBACK (gui_transform_mirror_clicked), rs);
	g_signal_connect ((gpointer) rot90, "clicked", G_CALLBACK (gui_transform_rot90_clicked), rs);
	g_signal_connect ((gpointer) rot180, "clicked", G_CALLBACK (gui_transform_rot180_clicked), rs);
	g_signal_connect ((gpointer) rot270, "clicked", G_CALLBACK (gui_transform_rot270_clicked), rs);
	gtk_widget_show (flip);
	gtk_widget_show (mirror);
	gtk_widget_show (rot90);
	gtk_widget_show (rot180);
	gtk_widget_show (rot270);
	gtk_box_pack_start(GTK_BOX (hbox), flip, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX (hbox), mirror, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX (hbox), rot270, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX (hbox), rot180, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX (hbox), rot90, FALSE, FALSE, 0);
	return(gui_box("Transforms", hbox));
}

gboolean
gui_tool_rgb_mixer_r_callback(GtkAdjustment *caller, RS_BLOB *rs)
{
	rs->settings[rs->current_setting]->rgb_mixer[R] = gtk_adjustment_get_value(caller);
	update_preview(rs);
	return(FALSE);
}

gboolean
gui_tool_rgb_mixer_g_callback(GtkAdjustment *caller, RS_BLOB *rs)
{
	rs->settings[rs->current_setting]->rgb_mixer[G] = gtk_adjustment_get_value(caller);
	update_preview(rs);
	return(FALSE);
}

gboolean
gui_tool_rgb_mixer_b_callback(GtkAdjustment *caller, RS_BLOB *rs)
{
	rs->settings[rs->current_setting]->rgb_mixer[B] = gtk_adjustment_get_value(caller);
	update_preview(rs);
	return(FALSE);
}

GtkWidget *
gui_tool_rgb_mixer(RS_BLOB *rs)
{
	GtkWidget *box;
	GtkWidget *rscale, *gscale, *bscale;

	rscale = gui_make_scale(rs, G_CALLBACK(gui_tool_rgb_mixer_r_callback), 1.0, 0.0, 5.0, 0.1, 0.5);
	gscale = gui_make_scale(rs, G_CALLBACK(gui_tool_rgb_mixer_g_callback), 1.0, 0.0, 5.0, 0.1, 0.5);
	bscale = gui_make_scale(rs, G_CALLBACK(gui_tool_rgb_mixer_b_callback), 1.0, 0.0, 5.0, 0.1, 0.5);
	box = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (box), rscale, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (box), gscale, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (box), bscale, FALSE, FALSE, 0);
	return(gui_box("RGB mixer", box));
}

GtkWidget *
gui_slider(GtkObject *adj, const gchar *label)
{
	GtkWidget *hscale;
	hscale = gtk_hscale_new (GTK_ADJUSTMENT (adj));
	gtk_scale_set_value_pos( GTK_SCALE(hscale), GTK_POS_LEFT);
	gtk_scale_set_digits(GTK_SCALE(hscale), 2);
	return(gui_box(label, hscale));
}

void
gui_reset_clicked(GtkWidget *w, RS_BLOB *rs)
{
	rs_reset(rs);
}

GtkWidget *
gui_reset(RS_BLOB *rs)
{
	GtkWidget *button;
	button = gtk_button_new_with_mnemonic ("Reset");
	g_signal_connect ((gpointer) button, "clicked", G_CALLBACK (gui_reset_clicked), rs);
	gtk_widget_show (button);
	return(button);
}

void
save_file_clicked(GtkWidget *w, RS_BLOB *rs)
{
	GtkWidget *fc;
	GString *name;
	gchar *dirname;
	gchar *basename;
	if (!rs->in_use) return;
	dirname = g_path_get_dirname(rs->filename);
	basename = g_path_get_basename(rs->filename);
	gui_status_push("Saving file ...");
	name = g_string_new(basename);
	g_string_append(name, "_output.png");

	fc = gtk_file_chooser_dialog_new ("Save File", NULL,
		GTK_FILE_CHOOSER_ACTION_SAVE,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT, NULL);

	gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (fc), TRUE);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (fc), dirname);
	gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (fc), name->str);
	if (gtk_dialog_run (GTK_DIALOG (fc)) == GTK_RESPONSE_ACCEPT)
	{
		char *filename;
		GdkPixbuf *pixbuf;

		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (fc));
		gtk_widget_destroy(fc);
		pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, rs->scaled->w, rs->scaled->h);
		rs_render(rs->mati, rs->scaled->w, rs->scaled->h, rs->scaled->pixels,
			rs->scaled->rowstride, rs->scaled->channels,
			gdk_pixbuf_get_pixels(pixbuf), gdk_pixbuf_get_rowstride(pixbuf));
		gdk_pixbuf_save(pixbuf, filename, "png", NULL, NULL);
		g_object_unref(pixbuf);
		g_free (filename);
	} else
		gtk_widget_destroy(fc);
	g_free(dirname);
	g_free(basename);
	g_string_free(name, TRUE);
	gui_status_pop();
	return;
}

GtkWidget *
save_file(RS_BLOB *rs)
{
	GtkWidget *button;
	button = gtk_button_new_with_mnemonic ("Save PNG");
	g_signal_connect ((gpointer) button, "clicked", G_CALLBACK (save_file_clicked), rs);
	gtk_widget_show (button);
	return(button);
}

GtkWidget *
gui_make_scale(RS_BLOB *rs, GCallback cb, double value, double min, double max, double step, double page)
{
	GtkWidget *hscale;
	GtkObject *adj;
	adj = gtk_adjustment_new(value, min, max, step, page, 0.0);
	gtk_signal_connect(GTK_OBJECT(adj), "value_changed",
		cb, rs);
	hscale = gtk_hscale_new (GTK_ADJUSTMENT (adj));
	gtk_scale_set_value_pos( GTK_SCALE(hscale), GTK_POS_LEFT);
	gtk_scale_set_digits(GTK_SCALE(hscale), 2);
	return(hscale);
}

gboolean
gui_tool_exposure_callback(GtkAdjustment *caller, RS_BLOB *rs)
{
	rs->settings[rs->current_setting]->exposure = gtk_adjustment_get_value(caller);
	update_preview(rs);
	return(FALSE);
}

GtkWidget *
gui_tool_exposure(RS_BLOB *rs)
{
	GtkWidget *hscale;
	GtkAdjustment *adj;
	adj = (GtkAdjustment *) gtk_adjustment_new(0.0, -2.0, 2.0, 0.1, 0.5, 0.0);
	gtk_signal_connect(GTK_OBJECT(adj), "value_changed",
		G_CALLBACK(gui_tool_exposure_callback), rs);
	hscale = gtk_hscale_new (GTK_ADJUSTMENT (adj));
	gtk_scale_set_value_pos( GTK_SCALE(hscale), GTK_POS_LEFT);
	gtk_scale_set_digits(GTK_SCALE(hscale), 2);
	return(gui_box("Exposure", hscale));
}

gboolean
gui_tool_saturation_callback(GtkAdjustment *caller, RS_BLOB *rs)
{
	rs->settings[rs->current_setting]->saturation = gtk_adjustment_get_value(caller);
	update_preview(rs);
	return(FALSE);
}

GtkWidget *
gui_tool_saturation(RS_BLOB *rs)
{
	GtkWidget *hscale;
	hscale = gui_make_scale(rs, G_CALLBACK(gui_tool_saturation_callback), 1.0, 0.0, 3.0, 0.1, 0.5);
	return(gui_box("Saturation", hscale));
}

gboolean
gui_tool_hue_callback(GtkAdjustment *caller, RS_BLOB *rs)
{
	rs->settings[rs->current_setting]->hue = gtk_adjustment_get_value(caller);
	update_preview(rs);
	return(FALSE);
}

GtkWidget *
gui_tool_hue(RS_BLOB *rs)
{
	GtkWidget *hscale;
	hscale = gui_make_scale(rs, G_CALLBACK(gui_tool_hue_callback), 0.0, 0.0, 360.0, 0.1, 30.0);
	return(gui_box("Hue", hscale));
}

gboolean
gui_tool_contrast_callback(GtkAdjustment *caller, RS_BLOB *rs)
{
	rs->settings[rs->current_setting]->contrast = gtk_adjustment_get_value(caller);
	update_preview(rs);
	return(FALSE);
}

GtkWidget *
gui_tool_contrast(RS_BLOB *rs)
{
	GtkWidget *hscale;
	hscale = gui_make_scale(rs, G_CALLBACK(gui_tool_contrast_callback), 1.0, 0.0, 3.0, 0.1, 0.1);
	return(gui_box("Contrast", hscale));
}

gboolean
gui_tool_gamma_callback(GtkAdjustment *caller, RS_BLOB *rs)
{
	rs->settings[rs->current_setting]->gamma = gtk_adjustment_get_value(caller);
	update_preview(rs);
	return(FALSE);
}

GtkWidget *
gui_tool_gamma(RS_BLOB *rs)
{
	GtkWidget *hscale;
	hscale = gui_make_scale(rs, G_CALLBACK(gui_tool_gamma_callback), 2.2, 0.0, 3.0, 0.1, 0.1);
	return(gui_box("Gamma", hscale));
}

GtkWidget *
gui_make_tools(RS_BLOB *rs)
{
	GtkWidget *toolbox;

	toolbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (toolbox);
	gtk_box_pack_start (GTK_BOX (toolbox), gui_tool_exposure(rs), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (toolbox), gui_tool_saturation(rs), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (toolbox), gui_tool_hue(rs), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (toolbox), gui_tool_contrast(rs), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (toolbox), gui_tool_rgb_mixer(rs), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (toolbox), gui_tool_gamma(rs), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (toolbox), gui_transform(rs), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (toolbox), gui_reset(rs), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (toolbox), save_file(rs), FALSE, FALSE, 0);
	return(toolbox);
}
void
gui_notebook_callback(GtkNotebook *notebook, GtkNotebookPage *page, guint page_num, RS_BLOB *rs)
{
	rs->current_setting = page_num;
	update_preview(rs);
}

GtkWidget *
make_toolbox(RS_BLOB *rs)
{
	GtkWidget *toolbox;
	GtkWidget *notebook;
	GtkWidget *label1;
	GtkWidget *label2;
	GtkWidget *label3;

	label1 = gtk_label_new(" A ");
	label2 = gtk_label_new(" B ");
	label3 = gtk_label_new(" C ");

	notebook = gtk_notebook_new();
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), gui_make_tools(rs), label1);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), gui_make_tools(rs), label2);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), gui_make_tools(rs), label3);
	g_signal_connect(notebook, "switch-page", G_CALLBACK(gui_notebook_callback), rs);

	toolbox = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (toolbox), notebook, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (toolbox), gui_slider(rs->scale, "Scale"), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (toolbox), gui_hist(rs, "Histogram"), FALSE, FALSE, 0);
	return(toolbox);
}

gint
fill_model_compare_func (GtkTreeModel *model, GtkTreeIter *tia,
	GtkTreeIter *tib, gpointer userdata)
{
	gint ret;
	gchar *a, *b;

	gtk_tree_model_get(model, tia, TEXT_COLUMN, &a, -1);
	gtk_tree_model_get(model, tib, TEXT_COLUMN, &b, -1);
	ret = g_utf8_collate(a,b);
	g_free(a);
	g_free(b);
	return(ret);
}

void
fill_model(GtkListStore *store, const char *path)
{
	gchar *name;
	GtkTreeIter iter;
	GdkPixbuf *pixbuf;
	GError *error;
	GDir *dir;
	GtkTreeSortable *sortable;
	RS_FILETYPE *filetype;
	dir = g_dir_open(path, 0, &error);
	if (dir == NULL) return;
	
	rs_conf_set_string(CONF_LWD, path);
	
	gui_status_push("Opening directory ...");
	GUI_CATCHUP();

	g_dir_rewind(dir);

	gtk_list_store_clear(store);
	while((name = (gchar *) g_dir_read_name(dir)))
	{
		filetype = rs_filetype_get(name, TRUE);
		if (filetype)
			if (filetype->load)
			{
				GString *fullname;
				fullname = g_string_new(path);
				fullname = g_string_append(fullname, "/");
				fullname = g_string_append(fullname, name);
				pixbuf = NULL;
				if (filetype->thumb)
					pixbuf = filetype->thumb(fullname->str);
				gtk_list_store_prepend (store, &iter);
				if (pixbuf==NULL)
					pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, 64, 64);
				gtk_list_store_set (store, &iter,
					PIXBUF_COLUMN, pixbuf,
					TEXT_COLUMN, name,
					FULLNAME_COLUMN, fullname->str,
					-1);
				g_object_unref (pixbuf);
				g_string_free(fullname, FALSE);
			}
	}
	sortable = GTK_TREE_SORTABLE(store);
	gtk_tree_sortable_set_sort_func(sortable,
		TEXT_COLUMN,
		fill_model_compare_func,
		NULL,
		NULL);
	gtk_tree_sortable_set_sort_column_id(sortable, TEXT_COLUMN, GTK_SORT_ASCENDING);
	gui_status_pop();
}

void
icon_activated(GtkIconView *iconview, RS_BLOB *rs)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreePath *path;
	gchar *name;
	RS_FILETYPE *filetype;

	model = gtk_icon_view_get_model(iconview);
	gtk_icon_view_get_cursor(iconview, &path, NULL);
	if (path!=NULL)
	{
		if (gtk_tree_model_get_iter(model, &iter, path))
		{
			gtk_tree_model_get(model, &iter, FULLNAME_COLUMN, &name, -1);
			gui_status_push("Opening image ...");
			GUI_CATCHUP();
			if ((filetype = rs_filetype_get(name, TRUE)))
			{
				filetype->load(rs, name);
				rs_reset(rs);
			}
			update_preview(rs);
			gui_status_pop();
		}
		gtk_tree_path_free(path);
	}
}

GtkWidget *
make_iconbox(RS_BLOB *rs, GtkListStore *store)
{
	GtkWidget *scroller;
	GtkWidget *iconview;

	iconview = gtk_icon_view_new();

	gtk_icon_view_set_pixbuf_column (GTK_ICON_VIEW (iconview), PIXBUF_COLUMN);
	gtk_icon_view_set_text_column (GTK_ICON_VIEW (iconview), TEXT_COLUMN);
	gtk_icon_view_set_model (GTK_ICON_VIEW (iconview), GTK_TREE_MODEL (store));
	gtk_icon_view_set_columns(GTK_ICON_VIEW (iconview), 1000);
	gtk_widget_set_size_request (iconview, -1, 140);
	g_signal_connect((gpointer) iconview, "selection_changed",
		G_CALLBACK (icon_activated), rs);

	scroller = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroller),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
	gtk_container_add (GTK_CONTAINER (scroller), iconview);

	return(scroller);
}
gboolean
drawingarea_expose (GtkWidget *widget, GdkEventExpose *event, RS_BLOB *rs)
{
	GtkAdjustment *vadj;
	GtkAdjustment *hadj;
	vadj = gtk_viewport_get_vadjustment((GtkViewport *) widget->parent);
	hadj = gtk_viewport_get_hadjustment((GtkViewport *) widget->parent);
	rs->preview_exposed->h = (gint) vadj->page_size;
	rs->preview_exposed->y = (gint) vadj->value;
	rs->preview_exposed->w = (gint) hadj->page_size;
	rs->preview_exposed->x = (gint) hadj->value;
	update_preview_region(rs, event->area.x, event->area.y,
		event->area.width, event->area.height);
	return(TRUE);
}

void
gui_menu_open_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	GtkWidget *fc;
	GtkListStore *store = (GtkListStore *) callback_action;
	gchar *lwd = rs_conf_get_string(CONF_LWD);
	
	
	fc = gtk_file_chooser_dialog_new ("Open File", NULL,
		GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
	
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER (fc), lwd);
	
	if (gtk_dialog_run (GTK_DIALOG (fc)) == GTK_RESPONSE_ACCEPT)
	{
		char *filename;

		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (fc));
		gtk_widget_destroy (fc);
		fill_model(store, filename);
		g_free (filename);
	} else
		gtk_widget_destroy (fc);

	g_free(lwd);
	return;
}

void
gui_about()
{
	static GtkWidget *aboutdialog = NULL;
	const gchar *authors[] = {
		"Anders Brander <anders@brander.dk>",
		"Anders Kvist <anders@kvistmail.dk>",
	};
	if (!aboutdialog)
	{
		printf("buh!\n");
		aboutdialog = gtk_about_dialog_new ();
		gtk_about_dialog_set_version (GTK_ABOUT_DIALOG (aboutdialog), "0.1rc");
		gtk_about_dialog_set_name (GTK_ABOUT_DIALOG (aboutdialog), "Rawstudio");
		gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG (aboutdialog), "A raw image converter for GTK+/GNOME");
		gtk_about_dialog_set_website(GTK_ABOUT_DIALOG (aboutdialog), "http://rawstudio.org/");
		gtk_about_dialog_set_authors(GTK_ABOUT_DIALOG (aboutdialog), authors);
	}
	gtk_widget_show(aboutdialog);
	return;
}

GtkWidget *
gui_make_menubar(RS_BLOB *rs, GtkWidget *window, GtkListStore *store)
{
	GtkItemFactoryEntry menu_items[] = {
		{ "/_File", NULL, NULL, 0, "<Branch>"},
		{ "/File/_Open", "<CTRL>O", gui_menu_open_callback, (gint) store, "<StockItem>", GTK_STOCK_OPEN},
		{ "/File/_Quit", "<CTRL>Q", gtk_main_quit, 0, "<StockItem>", GTK_STOCK_QUIT},
		{ "/_Help", NULL, NULL, 0, "<LastBranch>"},
		{ "/_Help/About", NULL, gui_about, 0, "<StockItem>", GTK_STOCK_ABOUT},
	};
	static gint nmenu_items = sizeof (menu_items) / sizeof (menu_items[0]);
	GtkItemFactory *item_factory;
	GtkAccelGroup *accel_group;

	accel_group = gtk_accel_group_new ();
	item_factory = gtk_item_factory_new (GTK_TYPE_MENU_BAR, "<main>", accel_group);
	gtk_item_factory_create_items (item_factory, nmenu_items, menu_items, NULL);
	gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);
	return(gtk_item_factory_get_widget (item_factory, "<main>"));
}

int
gui_init(int argc, char **argv)
{
	GtkWidget *window;
	GtkWidget *scroller;
	GtkWidget *viewport;
	GtkWidget *vbox;
	GtkWidget *pane;
	GtkWidget *toolbox;
	GtkWidget *iconbox;
	GtkListStore *store;
	GtkWidget *menubar;
	RS_BLOB *rs;
	gchar *lwd;
	
	gtk_init(&argc, &argv);

	rs = rs_new();
	statusbar = (GtkStatusbar *) gtk_statusbar_new();

	toolbox = make_toolbox(rs);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_resize((GtkWindow *) window, 800, 600);
	gtk_window_set_title (GTK_WINDOW (window), "Rawstudio");
	g_signal_connect((gpointer) window, "delete_event", G_CALLBACK(gtk_main_quit), NULL);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox);
	gtk_container_add (GTK_CONTAINER (window), vbox);
	
	store = gtk_list_store_new (NUM_COLUMNS, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);
	iconbox = make_iconbox(rs, store);

	lwd = rs_conf_get_string(CONF_LWD);
	
	if (!lwd)
	 lwd = g_get_current_dir();
	
	fill_model(store, lwd);
	g_free(lwd);

	menubar = gui_make_menubar(rs, window, store);
	gtk_box_pack_start (GTK_BOX (vbox), menubar, FALSE, TRUE, 0);

	gtk_box_pack_start (GTK_BOX (vbox), iconbox, FALSE, TRUE, 0);

	pane = gtk_hpaned_new ();
	gtk_box_pack_start (GTK_BOX (vbox), pane, TRUE, TRUE, 0);
	gtk_widget_show (pane);
	scroller = gtk_scrolled_window_new (NULL, NULL);
	gtk_paned_pack1 (GTK_PANED (pane), scroller, TRUE, TRUE);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroller),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_show (scroller);

	gtk_paned_pack2 (GTK_PANED (pane), toolbox, FALSE, TRUE);

	viewport = gtk_viewport_new (NULL, NULL);
	gtk_container_add (GTK_CONTAINER (scroller), viewport);

	rs->preview_drawingarea = gtk_drawing_area_new();
	gtk_signal_connect (GTK_OBJECT (rs->preview_drawingarea), "expose-event",
		GTK_SIGNAL_FUNC (drawingarea_expose), rs);

	gtk_container_add (GTK_CONTAINER (viewport), (GtkWidget *) rs->preview_drawingarea);

	gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (statusbar), FALSE, TRUE, 0);

	gui_status_push("Ready");

	gtk_widget_show_all (window);
	gtk_main();
	return(0);
}
