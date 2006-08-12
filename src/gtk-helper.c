/*
 * Copyright (C) 2006 Anders Brander <anders@brander.dk> and 
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

#include <glib.h>
#include <gtk/gtk.h>
#include "color.h"
#include "matrix.h"
#include "rs-batch.h"
#include "rawstudio.h"
#include "conf_interface.h"
#include "gtk-interface.h"
#include "filename.h"
#include <gettext.h>
#include <lcms.h>

void gui_cms_in_profile_combobox_changed(GtkComboBox *combobox, gpointer user_data);
void gui_cms_di_profile_combobox_changed(GtkComboBox *combobox, gpointer user_data);
void gui_cms_ex_profile_combobox_changed(GtkComboBox *combobox, gpointer user_data);
void gui_cms_intent_combobox_changed(GtkComboBox *combobox, gpointer user_data);
void gui_cms_in_profile_button_clicked(GtkButton *button, gpointer user_data);
void gui_cms_di_profile_button_clicked(GtkButton *button, gpointer user_data);
void gui_cms_ex_profile_button_clicked(GtkButton *button, gpointer user_data);
void gui_cms_gamma_value_changed(GtkAdjustment *caller, RS_BLOB *rs);

gchar *color_profiles[] = {
	"*.icc", 
	"*.icm", 
	"*.ICC", 
	"*.ICM", 
	NULL
};

GtkWidget *gui_tooltip_no_window(GtkWidget *widget, gchar *tip_tip, gchar *tip_private)
{
	GtkWidget *e;
	GtkTooltips *tip;

	tip = gtk_tooltips_new();
	e = gtk_event_box_new();
	gtk_tooltips_set_tip(tip, e, tip_tip, tip_private);
	gtk_widget_show(widget);
	gtk_container_add(GTK_CONTAINER(e), widget);

	return e;
}

void gui_tooltip_window(GtkWidget *widget, gchar *tip_tip, gchar *tip_private)
{
	GtkTooltips *tip;

	tip = gtk_tooltips_new();
	gtk_tooltips_set_tip(tip, widget, tip_tip, tip_private);
	gtk_widget_show(widget);

	return;
}

gboolean
gui_save_png(GdkPixbuf *pixbuf, gchar *filename)
{
	return gdk_pixbuf_save(pixbuf, filename, "png", NULL, NULL);
}

gboolean
gui_save_jpg(GdkPixbuf *pixbuf, gchar *filename)
{
	gboolean ret;
	gchar *quality = rs_conf_get_string(CONF_EXPORT_JPEG_QUALITY);
	if (!quality)
	{
		rs_conf_set_string(CONF_EXPORT_JPEG_QUALITY, DEFAULT_CONF_EXPORT_JPEG_QUALITY);
		quality = rs_conf_get_string(CONF_EXPORT_JPEG_QUALITY);
	}
	ret = gdk_pixbuf_save(pixbuf, filename, "jpeg", NULL, "quality", quality, NULL);
	g_free(quality);
	return ret;
}

void
gui_batch_directory_entry_changed(GtkEntry *entry, gpointer user_data)
{
	rs_conf_set_string(CONF_BATCH_DIRECTORY, gtk_entry_get_text(entry));
	return;
}

void
gui_batch_filename_entry_changed(GtkEntry *entry, gpointer user_data)
{
	rs_conf_set_string(CONF_BATCH_FILENAME, gtk_entry_get_text(entry));
	return;
}

void
gui_batch_filetype_entry_changed(GtkEntry *entry, gpointer user_data)
{
	rs_conf_set_string(CONF_BATCH_FILETYPE, gtk_entry_get_text(entry));
	return;
}

void
gui_export_changed_helper(GtkLabel *label)
{
	gchar *parsed = NULL;
	gchar *directory;
	gchar *filename;
	gchar *filetype;
	GString *final;

	directory = rs_conf_get_string(CONF_EXPORT_DIRECTORY);
	filename = rs_conf_get_string(CONF_EXPORT_FILENAME);
	filetype = rs_conf_get_string(CONF_EXPORT_FILETYPE);

	parsed = filename_parse(filename, NULL);

	final = g_string_new("<small>");
	g_string_append(final, directory);
	g_free(directory);
	g_string_append(final, parsed);
	g_free(parsed);
	g_string_append(final, ".");
	g_string_append(final, filetype);
	g_free(filetype);
	g_string_append(final, "</small>");

	gtk_label_set_markup(label, final->str);

	g_string_free(final, TRUE);

	return;
}

void
gui_export_directory_entry_changed(GtkEntry *entry, gpointer user_data)
{
	GtkLabel *label = GTK_LABEL(user_data);
	rs_conf_set_string(CONF_EXPORT_DIRECTORY, gtk_entry_get_text(entry));

	gui_export_changed_helper(label);

	return;
}

void
gui_export_filename_entry_changed(GtkComboBox *combobox, gpointer user_data)
{
	GtkLabel *label = GTK_LABEL(user_data);
	rs_conf_set_string(CONF_EXPORT_FILENAME, gtk_combo_box_get_active_text(combobox));

	gui_export_changed_helper(label);

	return;
}

void
gui_cms_in_profile_combobox_changed(GtkComboBox *combobox, gpointer user_data)
{
	rs_conf_set_integer(CONF_CMS_IN_PROFILE_SELECTED, gtk_combo_box_get_active(GTK_COMBO_BOX(combobox)));
	return;
}

void
gui_cms_di_profile_combobox_changed(GtkComboBox *combobox, gpointer user_data)
{
	rs_conf_set_integer(CONF_CMS_DI_PROFILE_SELECTED, gtk_combo_box_get_active(GTK_COMBO_BOX(combobox)));
	return;
}

void
gui_cms_ex_profile_combobox_changed(GtkComboBox *combobox, gpointer user_data)
{
	rs_conf_set_integer(CONF_CMS_EX_PROFILE_SELECTED, gtk_combo_box_get_active(GTK_COMBO_BOX(combobox)));
	return;
}

void
gui_cms_intent_combobox_changed(GtkComboBox *combobox, gpointer user_data)
{
	gint active = gtk_combo_box_get_active(combobox);
	rs_conf_set_integer(CONF_CMS_INTENT, active);
	return;
}

void
gui_cms_in_profile_button_clicked(GtkButton *button, gpointer user_data)
{
	GtkWidget *fc;
	GtkWidget *combobox = GTK_WIDGET(user_data);
	GtkFileFilter *file_filter_all;
	GtkFileFilter *file_filter_color_profiles;
	gint n;

	fc = gtk_file_chooser_dialog_new (_("Select color profile"), NULL,
		GTK_FILE_CHOOSER_ACTION_OPEN,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(fc), GTK_RESPONSE_ACCEPT);

	file_filter_all = gtk_file_filter_new();
	file_filter_color_profiles = gtk_file_filter_new();

	n=0;
	while(color_profiles[n])
	{
		gtk_file_filter_add_pattern(file_filter_color_profiles, color_profiles[n]);
		n++;
	}

	gtk_file_filter_add_pattern(file_filter_all, "*");

	gtk_file_filter_set_name(file_filter_all, _("All files"));
	gtk_file_filter_set_name(file_filter_color_profiles, _("Color profiles (icc and icm)"));

	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(fc), file_filter_color_profiles);
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(fc), file_filter_all);
	if (gtk_dialog_run (GTK_DIALOG (fc)) == GTK_RESPONSE_ACCEPT)
	{
		gchar *filename;
		cmsHPROFILE color_profile;
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (fc));
		gtk_widget_destroy (fc);

		color_profile = cmsOpenProfileFromFile(filename, "r");
		if (color_profile)
		{
			cmsCloseProfile(color_profile);
			gtk_combo_box_append_text(GTK_COMBO_BOX(combobox), g_basename(filename));
			// FIXME: gtk_combo_box_set_active(GTK_COMBO_BOX(combobox), __SELECT_LAST__);
			rs_conf_add_string_to_list_string(CONF_CMS_IN_PROFILE_LIST, filename);
			rs_conf_set_integer(CONF_CMS_IN_PROFILE_SELECTED, gtk_combo_box_get_active(GTK_COMBO_BOX(combobox)));
		}
		else
		{
			GtkWidget *warning = gui_dialog_make_from_text(GTK_STOCK_DIALOG_WARNING,
				_("Not a valid color profile."), 
				_("The file you selected does not appear to be a valid color profile."));
			GtkWidget *ok_button = gtk_button_new_from_stock(GTK_STOCK_OK);
			gtk_dialog_add_action_widget(GTK_DIALOG(warning), ok_button, GTK_RESPONSE_ACCEPT);
			
			gtk_widget_show_all(warning);
			
        	if((gtk_dialog_run(GTK_DIALOG(warning)) == GTK_RESPONSE_ACCEPT))
	        {
				gtk_widget_destroy(warning);
				return;
	        }
    	    else
			{
				gtk_widget_destroy(warning);
				return;
			}
		}

		g_free (filename);
	} else
		gtk_widget_destroy (fc);

	return;
}

void
gui_cms_di_profile_button_clicked(GtkButton *button, gpointer user_data)
{
	GtkWidget *fc;
	GtkWidget *combobox = GTK_WIDGET(user_data);
	GtkFileFilter *file_filter_all;
	GtkFileFilter *file_filter_color_profiles;
	gint n;

	fc = gtk_file_chooser_dialog_new (_("Select color profile"), NULL,
		GTK_FILE_CHOOSER_ACTION_OPEN,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(fc), GTK_RESPONSE_ACCEPT);

	file_filter_all = gtk_file_filter_new();
	file_filter_color_profiles = gtk_file_filter_new();

	n=0;
	while(color_profiles[n])
	{
		gtk_file_filter_add_pattern(file_filter_color_profiles, color_profiles[n]);
		n++;
	}

	gtk_file_filter_add_pattern(file_filter_all, "*");

	gtk_file_filter_set_name(file_filter_all, _("All files"));
	gtk_file_filter_set_name(file_filter_color_profiles, _("Color profiles (icc and icm)"));

	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(fc), file_filter_color_profiles);
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(fc), file_filter_all);
	if (gtk_dialog_run (GTK_DIALOG (fc)) == GTK_RESPONSE_ACCEPT)
	{
		gchar *filename;
		cmsHPROFILE color_profile;
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (fc));
		gtk_widget_destroy (fc);

		color_profile = cmsOpenProfileFromFile(filename, "r");
		if (color_profile)
		{
			cmsCloseProfile(color_profile);
			gtk_combo_box_append_text(GTK_COMBO_BOX(combobox), g_basename(filename));
			// FIXME: gtk_combo_box_set_active(GTK_COMBO_BOX(combobox), __SELECT_LAST__);
			rs_conf_add_string_to_list_string(CONF_CMS_DI_PROFILE_LIST, filename);
			rs_conf_set_integer(CONF_CMS_DI_PROFILE_SELECTED, gtk_combo_box_get_active(GTK_COMBO_BOX(combobox)));
		}
		else
		{
			GtkWidget *warning = gui_dialog_make_from_text(GTK_STOCK_DIALOG_WARNING,
				_("Not a valid color profile."), 
				_("The file you selected does not appear to be a valid color profile."));
			GtkWidget *ok_button = gtk_button_new_from_stock(GTK_STOCK_OK);
			gtk_dialog_add_action_widget(GTK_DIALOG(warning), ok_button, GTK_RESPONSE_ACCEPT);
			
			gtk_widget_show_all(warning);
			
        	if((gtk_dialog_run(GTK_DIALOG(warning)) == GTK_RESPONSE_ACCEPT))
	        {
				gtk_widget_destroy(warning);
				return;
	        }
    	    else
			{
				gtk_widget_destroy(warning);
				return;
			}
		}

		g_free (filename);
	} else
		gtk_widget_destroy (fc);

	return;
}

void
gui_cms_ex_profile_button_clicked(GtkButton *button, gpointer user_data)
{
	GtkWidget *fc;
	GtkWidget *combobox = GTK_WIDGET(user_data);
	GtkFileFilter *file_filter_all;
	GtkFileFilter *file_filter_color_profiles;
	gint n;

	fc = gtk_file_chooser_dialog_new (_("Select color profile"), NULL,
		GTK_FILE_CHOOSER_ACTION_OPEN,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(fc), GTK_RESPONSE_ACCEPT);

	file_filter_all = gtk_file_filter_new();
	file_filter_color_profiles = gtk_file_filter_new();

	n=0;
	while(color_profiles[n])
	{
		gtk_file_filter_add_pattern(file_filter_color_profiles, color_profiles[n]);
		n++;
	}

	gtk_file_filter_add_pattern(file_filter_all, "*");

	gtk_file_filter_set_name(file_filter_all, _("All files"));
	gtk_file_filter_set_name(file_filter_color_profiles, _("Color profiles (icc and icm)"));

	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(fc), file_filter_color_profiles);
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(fc), file_filter_all);
	if (gtk_dialog_run (GTK_DIALOG (fc)) == GTK_RESPONSE_ACCEPT)
	{
		gchar *filename;
		cmsHPROFILE color_profile;
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (fc));
		gtk_widget_destroy (fc);

		color_profile = cmsOpenProfileFromFile(filename, "r");
		if (color_profile)
		{
			cmsCloseProfile(color_profile);
			gtk_combo_box_append_text(GTK_COMBO_BOX(combobox), g_basename(filename));
			// FIXME: gtk_combo_box_set_active(GTK_COMBO_BOX(combobox), __SELECT_LAST__);
			rs_conf_add_string_to_list_string(CONF_CMS_DI_PROFILE_LIST, filename);
			rs_conf_set_integer(CONF_CMS_DI_PROFILE_SELECTED, gtk_combo_box_get_active(GTK_COMBO_BOX(combobox)));
		}
		else
		{
			GtkWidget *warning = gui_dialog_make_from_text(GTK_STOCK_DIALOG_WARNING,
				_("Not a valid color profile."), 
				_("The file you selected does not appear to be a valid color profile."));
			GtkWidget *ok_button = gtk_button_new_from_stock(GTK_STOCK_OK);
			gtk_dialog_add_action_widget(GTK_DIALOG(warning), ok_button, GTK_RESPONSE_ACCEPT);
			
			gtk_widget_show_all(warning);
			
        	if((gtk_dialog_run(GTK_DIALOG(warning)) == GTK_RESPONSE_ACCEPT))
	        {
				gtk_widget_destroy(warning);
				return;
	        }
    	    else
			{
				gtk_widget_destroy(warning);
				return;
			}
		}

		g_free (filename);
	} else
		gtk_widget_destroy (fc);

	return;
}

void
gui_cms_gamma_value_changed(GtkAdjustment *caller, RS_BLOB *rs)
{
	rs_conf_set_double(CONF_CMS_GAMMA_VALUE, caller->value);
	rs->gamma = caller->value;
	update_preview(rs);
	return;
}

GtkWidget *
gui_preferences_make_cms_page(RS_BLOB *rs)
{
	gint temp_conf_gint;
	gdouble temp_conf_gdouble;
	GSList *temp_conf_gslist;
	GSList *temp_new_gslist = NULL;
	gchar *temp_conf_string;

	GtkWidget *cms_page;
	GtkWidget *cms_in_profile_hbox;
	GtkWidget *cms_in_profile_label;
	GtkWidget *cms_in_profile_combobox;
	GtkWidget *cms_in_profile_button;
	GtkWidget *cms_di_profile_hbox;
	GtkWidget *cms_di_profile_label;
	GtkWidget *cms_di_profile_combobox;
	GtkWidget *cms_di_profile_button;
	GtkWidget *cms_ex_profile_hbox;
	GtkWidget *cms_ex_profile_label;
	GtkWidget *cms_ex_profile_combobox;
	GtkWidget *cms_ex_profile_button;
	GtkWidget *cms_intent_hbox;
	GtkWidget *cms_intent_label;
	GtkWidget *cms_intent_combobox;
	GtkWidget *cms_gamma_hbox;
	GtkWidget *cms_gamma_label;
	GtkObject *cms_gamma_adj;
	GtkWidget *cms_gamma_value;
	
	cms_page = gtk_vbox_new(FALSE, 4);
	gtk_container_set_border_width (GTK_CONTAINER (cms_page), 6);

	cms_in_profile_hbox = gtk_hbox_new(FALSE, 0);
	cms_in_profile_label = gtk_label_new(_("Input profile"));
	gtk_misc_set_alignment(GTK_MISC(cms_in_profile_label), 0.0, 0.5);
	cms_in_profile_combobox = gtk_combo_box_entry_new_text();
	gtk_combo_box_append_text(GTK_COMBO_BOX(cms_in_profile_combobox), _("BuildInRGBProfile"));
	temp_conf_gslist = rs_conf_get_list_string(CONF_CMS_IN_PROFILE_LIST);
	rs_conf_get_integer(CONF_CMS_IN_PROFILE_SELECTED, &temp_conf_gint);

	while(temp_conf_gslist)
	{
		temp_conf_string = (gchar *) temp_conf_gslist->data;
		if (g_file_test(temp_conf_string, G_FILE_TEST_EXISTS))
		{
			cmsHPROFILE color_profile = cmsOpenProfileFromFile(temp_conf_string, "r");
			if (color_profile)
			{
				cmsCloseProfile(color_profile);	
				gtk_combo_box_append_text(GTK_COMBO_BOX(cms_in_profile_combobox), g_basename(temp_conf_string));
				temp_new_gslist = g_slist_append(temp_new_gslist, (gpointer *) temp_conf_string);
			}
		}
		else
			temp_conf_gint = 0;
		temp_conf_gslist = temp_conf_gslist->next;
	}

	rs_conf_set_list_string(CONF_CMS_IN_PROFILE_LIST, temp_new_gslist);
	g_slist_free(temp_new_gslist);
	temp_new_gslist = NULL;
	if (temp_conf_gint < 0)
		temp_conf_gint = 0;
	gtk_combo_box_set_active(GTK_COMBO_BOX(cms_in_profile_combobox), temp_conf_gint);
	cms_in_profile_button = gtk_button_new_from_stock(GTK_STOCK_OPEN);
	gtk_box_pack_start (GTK_BOX (cms_in_profile_hbox), cms_in_profile_label, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (cms_in_profile_hbox), cms_in_profile_combobox, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (cms_in_profile_hbox), cms_in_profile_button, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (cms_page), cms_in_profile_hbox, FALSE, TRUE, 0);

	cms_di_profile_hbox = gtk_hbox_new(FALSE, 0);
	cms_di_profile_label = gtk_label_new(_("Display profile"));
	gtk_misc_set_alignment(GTK_MISC(cms_di_profile_label), 0.0, 0.5);
	cms_di_profile_combobox = gtk_combo_box_entry_new_text();
	gtk_combo_box_append_text(GTK_COMBO_BOX(cms_di_profile_combobox), _("BuildInRGBProfile"));
	temp_conf_gslist = rs_conf_get_list_string(CONF_CMS_DI_PROFILE_LIST);
	rs_conf_get_integer(CONF_CMS_DI_PROFILE_SELECTED, &temp_conf_gint);

	while(temp_conf_gslist)
	{
		temp_conf_string = (gchar *) temp_conf_gslist->data;
		if (g_file_test(temp_conf_string, G_FILE_TEST_EXISTS))
		{
			cmsHPROFILE color_profile = cmsOpenProfileFromFile(temp_conf_string, "r");
			if (color_profile)
			{
				cmsCloseProfile(color_profile);	
				gtk_combo_box_append_text(GTK_COMBO_BOX(cms_di_profile_combobox), g_basename(temp_conf_string));
				temp_new_gslist = g_slist_append(temp_new_gslist, (gpointer *) temp_conf_string);
			}
		}
		else
			temp_conf_gint = 0;
		temp_conf_gslist = temp_conf_gslist->next;
	}

	rs_conf_set_list_string(CONF_CMS_DI_PROFILE_LIST, temp_new_gslist);
	g_slist_free(temp_new_gslist);
	temp_new_gslist = NULL;
	if (temp_conf_gint < 0)
		temp_conf_gint = 0;
	gtk_combo_box_set_active(GTK_COMBO_BOX(cms_di_profile_combobox), temp_conf_gint);
	cms_di_profile_button = gtk_button_new_from_stock(GTK_STOCK_OPEN);
	gtk_box_pack_start (GTK_BOX (cms_di_profile_hbox), cms_di_profile_label, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (cms_di_profile_hbox), cms_di_profile_combobox, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (cms_di_profile_hbox), cms_di_profile_button, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (cms_page), cms_di_profile_hbox, FALSE, TRUE, 0);

	cms_ex_profile_hbox = gtk_hbox_new(FALSE, 0);
	cms_ex_profile_label = gtk_label_new(_("Export profile"));
	gtk_misc_set_alignment(GTK_MISC(cms_ex_profile_label), 0.0, 0.5);
	cms_ex_profile_combobox = gtk_combo_box_entry_new_text();
	gtk_combo_box_append_text(GTK_COMBO_BOX(cms_ex_profile_combobox), _("BuildInRGBProfile"));
	temp_conf_gslist = rs_conf_get_list_string(CONF_CMS_EX_PROFILE_LIST);
	rs_conf_get_integer(CONF_CMS_EX_PROFILE_SELECTED, &temp_conf_gint);

	while(temp_conf_gslist)
	{
		temp_conf_string = (gchar *) temp_conf_gslist->data;
		if (g_file_test(temp_conf_string, G_FILE_TEST_EXISTS))
		{
			cmsHPROFILE color_profile = cmsOpenProfileFromFile(temp_conf_string, "r");
			if (color_profile)
			{
				cmsCloseProfile(color_profile);	
				gtk_combo_box_append_text(GTK_COMBO_BOX(cms_ex_profile_combobox), g_basename(temp_conf_string));
				temp_new_gslist = g_slist_append(temp_new_gslist, (gpointer *) temp_conf_string);
			}
		}
		else
			temp_conf_gint = 0;
		temp_conf_gslist = temp_conf_gslist->next;
	}

	rs_conf_set_list_string(CONF_CMS_EX_PROFILE_LIST, temp_new_gslist);
	g_slist_free(temp_new_gslist);
	if (temp_conf_gint < 0)
		temp_conf_gint = 0;
	gtk_combo_box_set_active(GTK_COMBO_BOX(cms_ex_profile_combobox), temp_conf_gint);
	cms_ex_profile_button = gtk_button_new_from_stock(GTK_STOCK_OPEN);
	gtk_box_pack_start (GTK_BOX (cms_ex_profile_hbox), cms_ex_profile_label, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (cms_ex_profile_hbox), cms_ex_profile_combobox, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (cms_ex_profile_hbox), cms_ex_profile_button, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (cms_page), cms_ex_profile_hbox, FALSE, TRUE, 0);

	cms_intent_hbox = gtk_hbox_new(FALSE, 0);
	cms_intent_label = gtk_label_new(_("Intent"));
	gtk_misc_set_alignment(GTK_MISC(cms_intent_label), 0.0, 0.5);
	cms_intent_combobox = gtk_combo_box_entry_new_text();
	gtk_combo_box_append_text(GTK_COMBO_BOX(cms_intent_combobox), _("Perceptual"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(cms_intent_combobox), _("Relative colormetric"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(cms_intent_combobox), _("Saturation"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(cms_intent_combobox), _("Absolute colormetric"));
	rs_conf_get_integer(CONF_CMS_INTENT, &temp_conf_gint);
	gtk_combo_box_set_active(GTK_COMBO_BOX(cms_intent_combobox), temp_conf_gint);
	gtk_box_pack_start (GTK_BOX (cms_intent_hbox), cms_intent_label, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (cms_intent_hbox), cms_intent_combobox, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (cms_page), cms_intent_hbox, FALSE, TRUE, 0);


	cms_gamma_hbox = gtk_hbox_new(FALSE, 0);
	cms_gamma_label = gtk_label_new(_("Gamma value:"));
	gtk_misc_set_alignment(GTK_MISC(cms_gamma_label), 0.0, 0.5);
	rs_conf_get_double(CONF_CMS_GAMMA_VALUE, &temp_conf_gdouble);
	if (temp_conf_gdouble <= 0)
	{
		temp_conf_gdouble = 1.0;
		rs_conf_set_double(CONF_CMS_GAMMA_VALUE, temp_conf_gdouble);
	}
	cms_gamma_adj = gtk_adjustment_new(temp_conf_gdouble, 0.0, 5.0, 0.1, 1.0, 1.0);
	cms_gamma_value = gtk_spin_button_new(GTK_ADJUSTMENT(cms_gamma_adj), 1, 2);
	gtk_box_pack_start (GTK_BOX (cms_gamma_hbox), cms_gamma_label, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (cms_gamma_hbox), cms_gamma_value, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (cms_page), cms_gamma_hbox, FALSE, TRUE, 0);

	g_signal_connect ((gpointer) cms_in_profile_combobox, "changed",
			G_CALLBACK(gui_cms_in_profile_combobox_changed), NULL);
	g_signal_connect ((gpointer) cms_di_profile_combobox, "changed",
			G_CALLBACK(gui_cms_di_profile_combobox_changed), NULL);
	g_signal_connect ((gpointer) cms_ex_profile_combobox, "changed",
			G_CALLBACK(gui_cms_ex_profile_combobox_changed), NULL);
			
	g_signal_connect ((gpointer) cms_intent_combobox, "changed",
			G_CALLBACK(gui_cms_intent_combobox_changed), NULL);
			
	g_signal_connect ((gpointer) cms_in_profile_button, "clicked",
			G_CALLBACK(gui_cms_in_profile_button_clicked), (gpointer) cms_in_profile_combobox);
	g_signal_connect ((gpointer) cms_di_profile_button, "clicked",
			G_CALLBACK(gui_cms_di_profile_button_clicked), (gpointer) cms_di_profile_combobox);
	g_signal_connect ((gpointer) cms_ex_profile_button, "clicked",
			G_CALLBACK(gui_cms_ex_profile_button_clicked), (gpointer) cms_ex_profile_combobox);


	g_signal_connect((gpointer) cms_gamma_adj, "value_changed", 
			G_CALLBACK(gui_cms_gamma_value_changed), rs);

	return cms_page;
}
