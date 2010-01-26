#include "rs-dcp-file.h"
#include "rs-dcp-factory.h"
#include "config.h"
#include "rs-utils.h"

#define DCP_FACTORY_DEFAULT_SEARCH_PATH PACKAGE_DATA_DIR "/" PACKAGE "/profiles/"

struct _RSDcpFactory {
	GObject parent;

	GList *profiles;
};

G_DEFINE_TYPE(RSDcpFactory, rs_dcp_factory, G_TYPE_OBJECT)

static void
rs_dcp_factory_class_init(RSDcpFactoryClass *klass)
{
}

static void
rs_dcp_factory_init(RSDcpFactory *factory)
{
	factory->profiles = NULL;
}

static void
load_profiles(RSDcpFactory *factory, const gchar *path)
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
			RSDcpFile *dcp = rs_dcp_file_new_from_file(filename);
			const gchar *model = rs_dcp_file_get_model(dcp);
			if (model)
			{
				factory->profiles = g_list_prepend(factory->profiles, dcp);
			}
		}

		g_free(filename);
	}

}

RSDcpFactory *
rs_dcp_factory_new(const gchar *search_path)
{
	RSDcpFactory *factory = g_object_new(RS_TYPE_DCP_FACTORY, NULL);

	load_profiles(factory, search_path);
	
	return factory;
}

void
rs_dcp_factory_append(RSDcpFactory *factory, const gchar *search_path)
{
	load_profiles(factory, search_path);
}

RSDcpFactory *
rs_dcp_factory_new_default(void)
{
	static RSDcpFactory *factory = NULL;
	GStaticMutex lock = G_STATIC_MUTEX_INIT;

	g_static_mutex_lock(&lock);
	if (!factory)
	{
		factory = rs_dcp_factory_new(DCP_FACTORY_DEFAULT_SEARCH_PATH);

		gchar *user_profiles = g_strdup_printf("%s/profiles/", rs_confdir_get());
		rs_dcp_factory_append(factory, user_profiles);
		g_free(user_profiles);
	}
	g_static_mutex_unlock(&lock);

	return factory;
}

GList *
rs_dcp_factory_get_compatible(RSDcpFactory *factory, const gchar *make, const gchar *model)
{
	GList *matches = NULL;
	GList *node;

	for (node = g_list_first(factory->profiles) ; node != NULL ; node = g_list_next(node))
	{
		RSDcpFile *dcp = RS_DCP_FILE(node->data);

		if (model && g_str_equal(model, rs_dcp_file_get_model(dcp)))
			matches = g_list_prepend(matches, dcp);
	}

	return matches;
}

RSDcpFile *
rs_dcp_factory_find_from_id(RSDcpFactory *factory, const gchar *id)
{
	RSDcpFile *ret = NULL;
	GList *node;

	for (node = g_list_first(factory->profiles) ; node != NULL ; node = g_list_next(node))
	{
		RSDcpFile *dcp = RS_DCP_FILE(node->data);

		const gchar *dcp_id = rs_dcp_get_id(dcp);

		if (g_str_equal(id, dcp_id))
		{
			if (ret)
				g_warning("WARNING: Duplicate profiles detected in file: %s, for %s, named:%s.\nUnsing last found profile.", rs_tiff_get_filename_nopath(RS_TIFF(dcp)),  rs_dcp_file_get_model(dcp),  rs_dcp_file_get_name(dcp));
			ret = dcp;
		}
	}

	return ret;
}
