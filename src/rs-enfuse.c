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

      GdkPixbuf *pixbuf = rs_filter_response_get_image8(response);

      guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);
      gint rowstride = gdk_pixbuf_get_rowstride(pixbuf);
      gint height = gdk_pixbuf_get_height(pixbuf);

      gint i;
      gulong sum = 0;
      gint num = 0;

      for (i = 0; i<rowstride*height; i++)
	{
	  sum += pixels[i];
	  num++;
	}
      return (gint) (sum/num);
}

gint export_image(gchar *filename, RSOutput *output, RSFilter *filter, gint snapshot, double exposure, gchar *outputname, gint boundingbox) {

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
	rs_filter_set_recursive(filter,
				"image", photo->input_response,
				"filename", photo->filename,
				NULL);

      if (g_object_class_find_property(G_OBJECT_GET_CLASS(output), "filename"))
	g_object_set(output, "filename", outputname, NULL);

      rs_output_set_from_conf(output, "batch");
      rs_output_execute(output, filter);

      gint value = calculate_lightness(filter);
      printf("%s: %d\n", filename, value);
      g_list_free(filters);
      return value;
    }
  else
    return -1;
}

GList * export_images(GList *files, gboolean extend, gint dark, gfloat darkstep, gint bright, gfloat brightstep, gint boundingbox)
{
  gint num_selected = g_list_length(files);
  gint i;
  gchar *name;

  GString *output_str = g_string_new(g_get_tmp_dir());
  output_str = g_string_append(output_str, "/");
  output_str = g_string_append(output_str, ".rawstudio-enfuse-");
  GString *output_unique = NULL;

  RSFilter *finput = rs_filter_new("RSInputImage16", NULL);
  RSFilter *fdemosaic = rs_filter_new("RSDemosaic", finput);
  RSFilter *ffujirotate = rs_filter_new("RSFujiRotate", fdemosaic);
  RSFilter *frotate = rs_filter_new("RSRotate", ffujirotate);
  RSFilter *fcrop = rs_filter_new("RSCrop", frotate);
  RSFilter *ftransform_input = rs_filter_new("RSColorspaceTransform", fcrop);
  RSFilter *fdcp= rs_filter_new("RSDcp", ftransform_input);
  RSFilter *fcache = rs_filter_new("RSCache", fdcp);
  RSFilter *fresample= rs_filter_new("RSResample", fcache);
  RSFilter *fdenoise= rs_filter_new("RSDenoise", fresample);
  RSFilter *ftransform_display = rs_filter_new("RSColorspaceTransform", fdenoise);
  RSFilter *fend = ftransform_display;
  
  RSOutput *output = rs_output_new("RSTifffile");

  GList *exported_names = NULL;

  if (g_object_class_find_property(G_OBJECT_GET_CLASS(output), "uncompressed"))
    g_object_set(output, "uncompressed", FALSE, NULL);
  if (g_object_class_find_property(G_OBJECT_GET_CLASS(output), "save16bit"))
    g_object_set(output, "save16bit", TRUE, NULL);
  if (g_object_class_find_property(G_OBJECT_GET_CLASS(output), "copy-metadata"))
    g_object_set(output, "copy-metadata", FALSE, NULL);

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
	  output_unique = g_string_append(output_unique, ".tif");
	  lightness = export_image(name, output, fend, 0, 0.0, output_unique->str, boundingbox); /* FIXME: snapshot hardcoded */
  	  exported_names = g_list_append(exported_names, output_unique->str);

	  if (lightness > brightval)
	    {
	      brightval = lightness;
	      brightest = name;
	    }

	  if (lightness < darkval)
	    {
	      darkval = lightness;
	      darkest = name;
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
	  output_unique = g_string_append(output_unique, ".tif");
	  exported_names = g_list_append(exported_names, output_unique->str);
	  export_image(darkest, output, fend, 0, (darkstep*n*-1), output_unique->str, boundingbox); /* FIXME: snapshot hardcoded */
	  i++;
	}
      for (n = 1; n <= bright; n++)
	{
	  output_unique = g_string_new(output_str->str);
	  g_string_append_printf(output_unique, "%d", i);
	  g_string_append_printf(output_unique, "_%.1f", (brightstep*n));
	  output_unique = g_string_append(output_unique, ".tif");
	  exported_names = g_list_append(exported_names, output_unique->str);
	  export_image(brightest, output, fend, 0, (brightstep*n), output_unique->str, boundingbox); /* FIXME: snapshot hardcoded */
	  i++;
	}
    }
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
    }
}

gchar * rs_enfuse(GList *files)
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
  gint boundingbox = 500;

  if (num_selected == 1)
    extend = TRUE;

  if (g_list_length(files))
    {
      for(i=0; i<num_selected; i++)
	{
	  name = (gchar*) g_list_nth_data(files, i);
	  file = g_malloc(sizeof(char)*strlen(name));
	  sscanf(g_path_get_basename(name), "%[^.]", file);
	  outname = g_string_append(outname, file);
	  if (i+1 != num_selected)
	    outname = g_string_append(outname, "+");
	}
      fullpath = g_string_new(g_path_get_dirname(name));
      fullpath = g_string_append(fullpath, "/");
      fullpath = g_string_append(fullpath, outname->str);
      fullpath = g_string_append(fullpath, ".tif");
    }

  GList *exported_names = export_images(files, extend, 2, 1.0, 2, 1.0, boundingbox);
  GList *aligned_names = NULL;
  if (has_align_image_stack() && num_selected > 1)
      aligned_names = align_images(exported_names, align_options);
  else
      aligned_names = exported_names;
  enfuse_images(aligned_names, fullpath->str, enfuse_options);

  // FIXME: Aparantly something goes wrong if we copy exifdata...
  //  rs_exif_copy(first, fullpath->str, "sRGB", RS_EXIF_FILE_TYPE_TIFF);

  return fullpath->str;
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
