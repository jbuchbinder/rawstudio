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

#include <rawstudio.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <string.h>
#include <config.h>
#include <gettext.h>
#include "rs-enfuse.h"
#include <stdlib.h>
#include <fcntl.h>
#include "filename.h"
#include <rs-store.h>
#include "gtk-helper.h"
#include "rs-photo.h"
#include "rs-cache.h"
#include "gtk-progress.h"

gboolean has_align_image_stack ();

gint calculate_lightness(RSFilter *filter)
{
      RSFilterRequest *request = rs_filter_request_new();
      rs_filter_request_set_quick(RS_FILTER_REQUEST(request), TRUE);
      rs_filter_param_set_object(RS_FILTER_PARAM(request), "colorspace", rs_color_space_new_singleton("RSSrgb"));
      rs_filter_request_set_quick(RS_FILTER_REQUEST(request), TRUE);

      rs_filter_set_recursive(filter,
			      "bounding-box", TRUE,
			      "width", 256,
			      "height", 256,
			      NULL);

      RSFilterResponse *response = rs_filter_get_image8(filter, request);
      g_object_unref(request);

      if(!rs_filter_response_has_image8(response))
	return 127;

      GdkPixbuf *pixbuf = rs_filter_response_get_image8(response);
      g_object_unref(response);

      guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);
      gint rowstride = gdk_pixbuf_get_rowstride(pixbuf);
      gint height = gdk_pixbuf_get_height(pixbuf);
      gint width = gdk_pixbuf_get_width(pixbuf);
      gint channels = gdk_pixbuf_get_n_channels(pixbuf);

      gint x,y,c;
      gulong sum = 0;
      gint num = 0;

      for (y = 0; y < height; y++)
        {
	  for (x = 0; x < width; x++)
	    {
	      for (c = 0; c < channels; c++)
		{
		  sum += pixels[x*c+y*rowstride];
		}
	    }
	}

      g_object_unref(pixbuf);

      num = width*height*channels;
      return (gint) (sum/num);
}

gint export_image(gchar *filename, RSOutput *output, RSFilter *filter, gint snapshot, double exposure, gchar *outputname, gint boundingbox, RSFilter *resample) {

  RS_PHOTO *photo = rs_photo_load_from_file(filename);
  if (photo)
    {
      rs_metadata_load_from_file(photo->metadata, filename);
      rs_cache_load(photo);

      GList *filters = g_list_append(NULL, filter);
      rs_photo_set_exposure(photo, 0, exposure);
      rs_photo_apply_to_filters(photo, filters, snapshot);
      
      if (boundingbox > 0) 
	rs_filter_set_recursive(filter,
				"image", photo->input_response,
				"filename", photo->filename,
				"bounding-box", TRUE,
				"width", boundingbox,
				"height", boundingbox,
				NULL);
      else
	{
	  rs_filter_set_enabled(resample, FALSE);
	  rs_filter_set_recursive(filter,
				  "image", photo->input_response,
				  "filename", photo->filename,
				  NULL);
	}

      if (g_object_class_find_property(G_OBJECT_GET_CLASS(output), "filename"))
	g_object_set(output, "filename", outputname, NULL);

      rs_output_set_from_conf(output, "batch");
      rs_output_execute(output, filter);
      g_object_unref(photo);

      gint value = calculate_lightness(filter);
      printf("%s: %d\n", filename, value);
      g_list_free(filters);
      return value;
    }
  else
    return -1;
}

GList * export_images(RS_BLOB *rs, GList *files, gboolean extend, gint dark, gfloat darkstep, gint bright, gfloat brightstep, gint boundingbox)
{
  gint num_selected = g_list_length(files);
  gint i = 0;
  gchar *name;

  GString *output_str = g_string_new(g_get_tmp_dir());
  output_str = g_string_append(output_str, "/");
  output_str = g_string_append(output_str, ".rawstudio-enfuse-");
  GString *output_unique = NULL;

  /* a simple chain - we wanna use the "original" image with only white balance corrected and nothing else to get the best result */
  RSFilter *ftransform_input = rs_filter_new("RSColorspaceTransform", rs->filter_demosaic_cache);
  RSFilter *fdcp= rs_filter_new("RSDcp", ftransform_input);
  RSFilter *fresample= rs_filter_new("RSResample", fdcp);
  RSFilter *ftransform_display = rs_filter_new("RSColorspaceTransform", fdcp);
  RSFilter *fend = ftransform_display;
  
  RSOutput *output = rs_output_new("RSPngfile");

  GList *exported_names = NULL;

  if (g_object_class_find_property(G_OBJECT_GET_CLASS(output), "save16bit"))
    g_object_set(output, "save16bit", TRUE, NULL); /* We get odd results if we use 16 bit output - probably due to liniearity */
  if (g_object_class_find_property(G_OBJECT_GET_CLASS(output), "copy-metadata"))
    g_object_set(output, "copy-metadata", TRUE, NULL); /* Doesn't make sense to enable - Enfuse doesn't copy it */

  gint lightness = 0;
  gint darkval = 255;
  gint brightval = 0;
  gchar *darkest = NULL;
  gchar *brightest = NULL;

  num_selected = g_list_length(files);
  if (g_list_length(files))
    {
      for(i=0; i<num_selected; i++)
	{
	  name = (gchar*) g_list_nth_data(files, i);
	  output_unique = g_string_new(output_str->str);
	  g_string_append_printf(output_unique, "%d", i);
	  output_unique = g_string_append(output_unique, ".png");
	  lightness = export_image(name, output, fend, 0, 0.0, output_unique->str, boundingbox, fresample); /* FIXME: snapshot hardcoded */
  	  exported_names = g_list_append(exported_names, g_strdup(output_unique->str));
	  g_string_free(output_unique, TRUE);

	  if (lightness > brightval)
	    {
	      brightval = lightness;
	      brightest = g_strdup(name);
	    }

	  if (lightness < darkval)
	    {
	      darkval = lightness;
	      darkest = g_strdup(name);
	    }
	}
    }

  if (extend)
    {
      gint n;
      for (n = 1; n <= dark; n++)
	{
	  output_unique = g_string_new(output_str->str);
	  g_string_append_printf(output_unique, "%d", i);
	  g_string_append_printf(output_unique, "_%.1f", (darkstep*n*-1));
	  output_unique = g_string_append(output_unique, ".png");
	  exported_names = g_list_append(exported_names, g_strdup(output_unique->str));
	  export_image(darkest, output, fend, 0, (darkstep*n*-1), output_unique->str, boundingbox, fresample); /* FIXME: snapshot hardcoded */
	  g_string_free(output_unique, TRUE);
	  i++;
	}
      g_free(darkest);
      for (n = 1; n <= bright; n++)
	{
	  output_unique = g_string_new(output_str->str);
	  g_string_append_printf(output_unique, "%d", i);
	  g_string_append_printf(output_unique, "_%.1f", (brightstep*n));
	  output_unique = g_string_append(output_unique, ".png");
	  exported_names = g_list_append(exported_names, g_strdup(output_unique->str));
	  export_image(brightest, output, fend, 0, (brightstep*n), output_unique->str, boundingbox, fresample); /* FIXME: snapshot hardcoded */
	  g_string_free(output_unique, TRUE);
	  i++;
	}
      g_free(brightest);
    }

  /* FIXME: shouldn't 'files' be freed here? It breaks RSStore... */

  return exported_names;
}

GList * align_images (GList *files, gchar *options) {
  gint num_selected = g_list_length(files);
  gint i;
  gchar *name;
  GList *aligned_names = NULL;

  if (g_list_length(files))
    {
      GString *command = g_string_new("align_image_stack -a /tmp/.rawstudio-enfuse-aligned- ");
      if (options)
         command = g_string_append(command, options);
      command = g_string_append(command, " ");
      for(i=0; i<num_selected; i++)
	{
	  name = (gchar*) g_list_nth_data(files, i);
	  command = g_string_append(command, name);
	  command = g_string_append(command, " ");
	  aligned_names = g_list_append(aligned_names, g_strdup_printf ("/tmp/.rawstudio-enfuse-aligned-%04d.tif", i));
	}
      printf("command: %s\n", command->str);
      if (system(command->str));
      g_string_free(command, TRUE);
      g_list_free(files);
    }
  return aligned_names;
}

void enfuse_images(GList *files, gchar *out, gchar *options) {
  gint num_selected = g_list_length(files);
  gint i;
  gchar *name;

  if (g_list_length(files))
    {
      GString *command = g_string_new("enfuse ");
      for(i=0; i<num_selected; i++)
	{
	  name = (gchar*) g_list_nth_data(files, i);
	  command = g_string_append(command, name);
	  command = g_string_append(command, " ");
	}
      command = g_string_append(command, options);
      command = g_string_append(command, " -o ");
      command = g_string_append(command, out);
      printf("command: %s\n", command->str);
      if(system(command->str));
      g_string_free(command, TRUE);
      g_list_free(files);
    }
}

gchar * rs_enfuse(RS_BLOB *rs, GList *files)
{
  gint num_selected = g_list_length(files);
  gint i;
  gchar *name = NULL;
  gchar *file = NULL;
  GString *outname = g_string_new("");
  GString *fullpath = NULL;
  gchar *align_options = NULL;
  gchar *enfuse_options = g_strdup("-d 16");
  gboolean extend = TRUE;
  gint extend_num = 1;
  gfloat extend_step = 2.0;
  gint boundingbox = 0;

  gchar *first = NULL;
  gchar *parsed_filename = NULL;

  RS_PROGRESS *progress = gui_progress_new("Enfusing...", 4);

  if (num_selected == 1)
    {
      extend = TRUE;
      extend_num = 3;
      extend_step = 1.0;
    }

  if (g_list_length(files))
    {
      for(i=0; i<num_selected; i++)
	{
	  name = (gchar*) g_list_nth_data(files, i);
	  if (first == NULL)
	    first = g_strdup(name);
	  file = g_malloc(sizeof(char)*strlen(name));
	  sscanf(g_path_get_basename(name), "%[^.]", file);
	  outname = g_string_append(outname, file);
	  g_free(file);
	  if (i+1 != num_selected)
	    outname = g_string_append(outname, "+");
	}
      fullpath = g_string_new(g_path_get_dirname(name));
      fullpath = g_string_append(fullpath, "/");
      fullpath = g_string_append(fullpath, outname->str);
      fullpath = g_string_append(fullpath, "_%2c");
      fullpath = g_string_append(fullpath, ".png");
      parsed_filename = filename_parse(fullpath->str, g_strdup(first), 0, FALSE);
      g_string_free(outname, TRUE);
      g_string_free(fullpath, TRUE);
    }

  g_usleep(500000); /* FIXME */
  gui_progress_advance_one(progress); /* 1 - initiate */

  GList *exported_names = export_images(rs, files, extend, extend_num, extend_step, extend_num, extend_step, boundingbox);

  gui_progress_advance_one(progress); /* 2 - after exported images */

  GList *aligned_names = NULL;
  if (has_align_image_stack() && num_selected > 1)
    {
      aligned_names = align_images(exported_names, align_options);
      g_free(align_options);
    }
  else
      aligned_names = exported_names;

  gui_progress_advance_one(progress); /* 3 - after aligned images */

  enfuse_images(aligned_names, parsed_filename, enfuse_options);

  gui_progress_advance_one(progress); /* 4 - after enfusing */

  g_free(enfuse_options);

  gui_progress_free(progress);

  /* FIXME: should use the photo in the middle as it's averaged between it... */
  rs_exif_copy(first, parsed_filename, "sRGB", RS_EXIF_FILE_TYPE_PNG);
  if (first)
    g_free(first);

  return parsed_filename;
}

gboolean rs_has_enfuse (gint major, gint minor)
{
  FILE *fp;
  char line1[128];
  char line2[128];
  int _major = 0, _minor = 0;
  gboolean retval = FALSE;

  fp = popen("enfuse -V","r"); /* enfuse 4.0-753b534c819d */
  if (fgets(line1, sizeof line1, fp) == NULL)
    {
      g_warning("fgets returned: %d\n", retval);
      return FALSE;
    }
  pclose(fp);

  fp = popen("enfuse -h","r"); /* ==== enfuse, version 3.2 ==== */
  if (fgets(line2, sizeof line2, fp) == NULL)
    {
      g_warning("fgets returned: %d\n", retval);
      return FALSE;
    }
  pclose(fp);

  GRegex *regex;
  gchar **tokens;

  regex = g_regex_new("(enfuse|.* enfuse, version) ([0-9])\x2E([0-9]+).*", 0, 0, NULL);
  tokens = g_regex_split(regex, line1, 0);
  if (tokens)
    {
      g_regex_unref(regex);
    }
  else 
    {
      tokens = g_regex_split(regex, line2, 0);
      g_regex_unref(regex);
      if (!tokens)
	return FALSE;
    }

  _major = atoi(tokens[2]);
  _minor = atoi(tokens[3]);

  if (_major > major) {
    retval = TRUE;
  } else if (_major == major) {
    if (_minor >= minor) {
      retval = TRUE;
    }
  }

  g_free(tokens);

  return retval;
}

gboolean has_align_image_stack ()
{
  FILE *fp;
  char line[128];
  gboolean retval = FALSE;

  fp = popen("align_image_stack 2>&1","r");
  if (fgets(line, sizeof line, fp) == NULL)
    {
      g_warning("fgets returned: %d\n", retval);
      return FALSE;
    }
  pclose(fp);
  return TRUE;
}
