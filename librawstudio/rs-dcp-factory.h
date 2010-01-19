#ifndef RS_DCP_FACTORY_H
#define RS_DCP_FACTORY_H

#include <glib-object.h>
#include "rs-dcp-file.h"

G_BEGIN_DECLS

#define RS_TYPE_DCP_FACTORY rs_dcp_factory_get_type()
#define RS_DCP_FACTORY(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_DCP_FACTORY, RSDcpFactory))
#define RS_DCP_FACTORY_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_DCP_FACTORY, RSDcpFactoryClass))
#define RS_IS_DCP_FACTORY(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_DCP_FACTORY))
#define RS_IS_DCP_FACTORY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RS_TYPE_DCP_FACTORY))
#define RS_DCP_FACTORY_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_TYPE_DCP_FACTORY, RSDcpFactoryClass))

enum {
	RS_DCP_FACTORY_STORE_MODEL,
	RS_DCP_FACTORY_STORE_DCP,
	RS_DCP_FACTORY_NUM_FIELDS
};

typedef struct _RSDcpFactory RSDcpFactory;

typedef struct {
	GObjectClass parent_class;
} RSDcpFactoryClass;

GType rs_dcp_factory_get_type(void);

RSDcpFactory *rs_dcp_factory_new(const gchar *search_path);

RSDcpFactory *rs_dcp_factory_new_default(void);

GList *rs_dcp_factory_get_compatible(RSDcpFactory *factory, const gchar *make, const gchar *model);

RSDcpFile *rs_dcp_factory_find_from_id(RSDcpFactory *factory, const gchar *path);

G_END_DECLS

#endif /* RS_DCP_FACTORY_H */
