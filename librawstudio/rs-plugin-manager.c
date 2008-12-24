/*
 * Copyright (C) 2006-2008 Anders Brander <anders@brander.dk> and 
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

#include <gmodule.h>
#include <rawstudio.h>
#include "config.h"
#include "rs-plugin.h"

static GList *plugins = NULL;

/**
 * Load all installed Rawstudio plugins
 */
gint
rs_plugin_manager_load_all_plugins()
{
	gint num = 0;
	gchar *plugin_directory;
	GDir *dir;
	const gchar *filename;
	GTimer *gt = g_timer_new();

	g_assert(g_module_supported());

	plugin_directory = g_build_filename(PACKAGE_DATA_DIR, PACKAGE, "plugins", NULL);
	g_debug("Loading modules from %s", plugin_directory);

	dir = g_dir_open(plugin_directory, 0, NULL);

	while(dir && (filename = g_dir_read_name(dir)))
	{
		if (g_str_has_suffix(filename, "." G_MODULE_SUFFIX))
		{
			RSPlugin *plugin;
			gchar *path;

			/* Load the plugin */
			path = g_build_filename(plugin_directory, filename, NULL);
			plugin = rs_plugin_new(path);
			g_free(path);

			g_assert(g_type_module_use(G_TYPE_MODULE(plugin)));
			g_type_module_unuse(G_TYPE_MODULE(plugin));

			plugins = g_list_prepend (plugins, plugin);

			g_debug("%s loaded", filename);
			num++;
		}
	}
	g_debug("%d plugins loaded in %.03f second", num, g_timer_elapsed(gt, NULL));

	g_debug("Filters loaded:");

	/* Print some debug info about loaded filters */
	GType *filters;
	guint n_filters, i;
	filters = g_type_children (RS_TYPE_FILTER, &n_filters);
	for (i = 0; i < n_filters; i++)
	{
		RSFilterClass *klass;
		klass = g_type_class_ref(filters[i]);
		g_debug("- %s", klass->name);
		g_type_class_unref(klass);
	}
	g_free(filters);

	if (dir)
		g_dir_close(dir);

	g_timer_destroy(gt);

	return num;
}
