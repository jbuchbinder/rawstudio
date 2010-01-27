#include "rs-dcp-file.h"
#include "rs-profile-factory.h"
#include "rs-profile-factory-model.h"
#include "config.h"
#include "rs-utils.h"

#define PROFILE_FACTORY_DEFAULT_SEARCH_PATH PACKAGE_DATA_DIR "/" PACKAGE "/profiles/"

struct _RSProfileFactory {
	GObject parent;

	GtkListStore *profiles;
};

G_DEFINE_TYPE(RSProfileFactory, rs_profile_factory, G_TYPE_OBJECT)

static void
rs_profile_factory_class_init(RSProfileFactoryClass *klass)
{
}

static void
rs_profile_factory_init(RSProfileFactory *factory)
{
	/* We use G_TYPE_POINTER to store some strings because they should live
	 forever - and we avoid unneeded strdup/free */
	factory->profiles = gtk_list_store_new(FACTORY_MODEL_NUM_COLUMNS, RS_TYPE_DCP_FILE, G_TYPE_POINTER, G_TYPE_POINTER);
}

static void
load_profiles(RSProfileFactory *factory, const gchar *path)
{
	const gchar *basename;
	gchar *filename;
	GDir *dir = g_dir_open(path, 0, NULL);

	while((dir != NULL) && (basename = g_dir_read_name(dir)))
	{
		if (basename[0] == '.')
            continue;

		filename = g_build_filename(path, basename, NULL);

		if (g_file_test(filename, G_FILE_TEST_IS_DIR))
			load_profiles(factory, filename);

		else if (g_file_test(filename, G_FILE_TEST_IS_REGULAR)
			&& (g_str_has_suffix(basename, ".dcp") || g_str_has_suffix(basename, ".DCP")))
		{
			RSDcpFile *profile = rs_dcp_file_new_from_file(filename);
			const gchar *model = rs_dcp_file_get_model(profile);
			if (model)
			{
				GtkTreeIter iter;

				gtk_list_store_prepend(factory->profiles, &iter);
				gtk_list_store_set(factory->profiles, &iter,
					FACTORY_MODEL_COLUMN_PROFILE, profile,
					FACTORY_MODEL_COLUMN_MODEL, model,
					FACTORY_MODEL_COLUMN_ID, rs_dcp_get_id(profile),
					-1);
			}
		}

		g_free(filename);
	}

}

RSProfileFactory *
rs_profile_factory_new(const gchar *search_path)
{
	RSProfileFactory *factory = g_object_new(RS_TYPE_PROFILE_FACTORY, NULL);

	load_profiles(factory, search_path);
	
	return factory;
}

void
rs_profile_factory_append(RSProfileFactory *factory, const gchar *search_path)
{
	load_profiles(factory, search_path);
}

RSProfileFactory *
rs_profile_factory_new_default(void)
{
	static RSProfileFactory *factory = NULL;
	GStaticMutex lock = G_STATIC_MUTEX_INIT;

	g_static_mutex_lock(&lock);
	if (!factory)
	{
		factory = rs_profile_factory_new(PROFILE_FACTORY_DEFAULT_SEARCH_PATH);

		gchar *user_profiles = g_strdup_printf("%s/profiles/", rs_confdir_get());
		rs_profile_factory_append(factory, user_profiles);
		g_free(user_profiles);
	}
	g_static_mutex_unlock(&lock);

	return factory;
}

GList *
rs_profile_factory_get_compatible(RSProfileFactory *factory, const gchar *make, const gchar *model)
{
	RSDcpFile *dcp;
	gchar *dcp_model;
	GList *matches = NULL;
	GtkTreeIter iter;
	GtkTreeModel *treemodel = GTK_TREE_MODEL(factory->profiles);

	if (gtk_tree_model_get_iter_first(treemodel, &iter))
		do {
			gtk_tree_model_get(treemodel, &iter,
				FACTORY_MODEL_COLUMN_MODEL, &dcp_model,
				FACTORY_MODEL_COLUMN_PROFILE, &dcp,
				-1);
			if (model && g_str_equal(model, dcp_model))
				matches = g_list_prepend(matches, dcp);
			g_object_unref(dcp);
		} while (gtk_tree_model_iter_next(treemodel, &iter));

	return matches;
}

RSDcpFile *
rs_profile_factory_find_from_id(RSProfileFactory *factory, const gchar *id)
{
	RSDcpFile *ret = NULL;
	RSDcpFile *dcp;
	gchar *model_id;
	GtkTreeIter iter;
	GtkTreeModel *treemodel = GTK_TREE_MODEL(factory->profiles);

	if (gtk_tree_model_get_iter_first(treemodel, &iter))
		do {
			gtk_tree_model_get(treemodel, &iter,
				FACTORY_MODEL_COLUMN_ID, &model_id,
				-1);

			if (id && g_str_equal(id, model_id))
			{
				gtk_tree_model_get(treemodel, &iter,
					FACTORY_MODEL_COLUMN_PROFILE, &dcp,
					-1);

				if (ret)
					g_warning("WARNING: Duplicate profiles detected in file: %s, for %s, named:%s.\nUnsing last found profile.", rs_tiff_get_filename_nopath(RS_TIFF(dcp)),  rs_dcp_file_get_model(dcp),  rs_dcp_file_get_name(dcp));

				ret = dcp;
				g_object_unref(dcp);
			}
		} while (gtk_tree_model_iter_next(treemodel, &iter));

	return ret;
}
