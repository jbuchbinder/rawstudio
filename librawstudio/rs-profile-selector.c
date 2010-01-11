#include "rs-profile-selector.h"
#include "rs-icc-profile.h"

G_DEFINE_TYPE(RSProfileSelector, rs_profile_selector, GTK_TYPE_COMBO_BOX)

enum {
    DCP_SELECTED_SIGNAL,
	ICC_SELECTED_SIGNAL,
    LAST_SIGNAL
};
static guint signals[LAST_SIGNAL] = {0};

enum {
	COLUMN_NAME,
	COLUMN_POINTER,
	COLUMN_TYPE,
	NUM_COLUMNS
};

static void
rs_profile_selector_dispose(GObject *object)
{
	G_OBJECT_CLASS(rs_profile_selector_parent_class)->dispose(object);
}

static void
rs_profile_selector_finalize(GObject *object)
{
	G_OBJECT_CLASS(rs_profile_selector_parent_class)->finalize(object);
}

static void
rs_profile_selector_class_init(RSProfileSelectorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	signals[DCP_SELECTED_SIGNAL] = g_signal_new("dcp-selected",
		G_TYPE_FROM_CLASS(klass),
	    G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		0,
		NULL,
		NULL,
        g_cclosure_marshal_VOID__OBJECT,
        G_TYPE_NONE, 1, RS_TYPE_DCP_FILE);

	signals[ICC_SELECTED_SIGNAL] = g_signal_new("icc-selected",
		G_TYPE_FROM_CLASS(klass),
	    G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		0,
		NULL,
		NULL,
        g_cclosure_marshal_VOID__OBJECT,
        G_TYPE_NONE, 1, RS_TYPE_ICC_PROFILE);

	object_class->dispose = rs_profile_selector_dispose;
	object_class->finalize = rs_profile_selector_finalize;
}

static void
changed(GtkComboBox *combo, gpointer data)
{
	GtkTreeIter iter;
	GType type;
	gpointer profile;
	GtkTreeModel *model;

	if (gtk_combo_box_get_active_iter(combo, &iter))
	{
		model = gtk_combo_box_get_model(combo);

		gtk_tree_model_get(model, &iter,
			COLUMN_POINTER, &profile,
			COLUMN_TYPE, &type,
			-1);

		if (type == RS_TYPE_DCP_FILE)
			g_signal_emit(RS_PROFILE_SELECTOR(combo), signals[DCP_SELECTED_SIGNAL], 0, profile);
		else if (type == RS_TYPE_ICC_PROFILE)
			g_signal_emit(RS_PROFILE_SELECTOR(combo), signals[ICC_SELECTED_SIGNAL], 0, profile);
	}
}

static void
rs_profile_selector_init(RSProfileSelector *selector)
{
	GtkComboBox *combo = GTK_COMBO_BOX(selector);

	selector->store = gtk_list_store_new(NUM_COLUMNS, G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_GTYPE);

	g_signal_connect(combo, "changed", G_CALLBACK(changed), NULL);

	gtk_combo_box_set_model(combo, GTK_TREE_MODEL(selector->store));

	GtkCellRenderer *cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), cell, TRUE );
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo), cell,
		"text", 0,
		NULL);
}

RSProfileSelector *
rs_profile_selector_new(void)
{
	return g_object_new(RS_TYPE_PROFILE_SELECTOR, NULL);
}

void
rs_profile_selector_select_profile(RSProfileSelector *selector, gpointer profile)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gpointer current = NULL;

	g_assert(RS_IS_PROFILE_SELECTOR(selector));

	model = GTK_TREE_MODEL(selector->store);

	if (gtk_tree_model_get_iter_first(model, &iter))
		do {
			gtk_tree_model_get(model, &iter,
				COLUMN_POINTER, &current,
				-1);
			if (current == profile)
			{
				gtk_combo_box_set_active_iter(GTK_COMBO_BOX(selector), &iter);
				break;
			}
		} while (gtk_tree_model_iter_next(model, &iter));
}

void
rs_profile_selector_set_dcp_profile(RSProfileSelector *selector, RSDcpFile *dcp)
{
	GtkTreeIter iter;

	g_assert(RS_IS_PROFILE_SELECTOR(selector));
	g_assert(RS_IS_DCP_FILE(dcp));

	gtk_list_store_append(selector->store, &iter);
	gtk_list_store_set(selector->store, &iter,
		COLUMN_NAME, rs_dcp_file_get_name(dcp),
		COLUMN_POINTER, dcp,
		COLUMN_TYPE, RS_TYPE_DCP_FILE,
		-1);
}

void
rs_profile_selector_set_profiles(RSProfileSelector *selector, GList *profiles)
{
	GList *node;
	g_assert(RS_IS_PROFILE_SELECTOR(selector));

	gtk_list_store_clear(selector->store);

	for (node=g_list_first(profiles) ; node!=NULL ; node = g_list_next(node))
	{
		if (RS_IS_DCP_FILE(node->data))
			rs_profile_selector_set_dcp_profile(selector, node->data);
		/* FIXME: Add support for ICC profiles */
	}
}

void
rs_profile_selector_set_profiles_steal(RSProfileSelector *selector, GList *profiles)
{
	rs_profile_selector_set_profiles(selector, profiles);

}
