#include <gdk-pixbuf/gdk-pixbuf.h>
#include <json-glib/json-glib.h>

#include "pidginabout.h"
#include "pidginresources.h"
#include "internal.h"

#include <stdio.h>

#include "config.h"
#ifdef HAVE_MESON_CONFIG
#include "meson-config.h"
#else
#error HAVE_MESON_CONFIG is not defined
#endif

struct _PidginAboutDialogPrivate {
	GtkWidget *stack;

	GtkWidget *credits_button;
	GtkWidget *credits_page;
	GtkWidget *credits_treeview;
	GtkTreeStore *credits_store;

	GtkWidget *build_info_button;
	GtkWidget *build_info_page;
	GtkWidget *build_info_treeview;
	GtkTreeStore *build_info_store;

	gboolean switching_pages;
};

/******************************************************************************
 * Helpers
 *****************************************************************************/
static void
_pidgin_about_dialog_switch_page(PidginAboutDialog *about, const gchar *name) {
	about->priv->switching_pages = TRUE;

	gtk_stack_set_visible_child_name(GTK_STACK(about->priv->stack), name);

	/* now figure out if credits button is active */
	gtk_toggle_button_set_active(
		GTK_TOGGLE_BUTTON(about->priv->credits_button),
		g_str_equal("credits", name)
	);

	/* is the build info button active? */
	gtk_toggle_button_set_active(
		GTK_TOGGLE_BUTTON(about->priv->build_info_button),
		g_str_equal("build-info", name)
	);

	about->priv->switching_pages = FALSE;
}

static void
_pidgin_about_dialog_load_contributors(PidginAboutDialog *about) {
	GInputStream *istream = NULL;
	GList *l = NULL, *sections = NULL;
	GError *error = NULL;
	JsonParser *parser = NULL;
	JsonNode *root_node = NULL;
	JsonObject *root_object = NULL;
	JsonArray *credits = NULL;

	/* get a stream to the credits resource */
	istream = g_resource_open_stream(
		pidgin_get_resource(),
		"/im/pidgin/Pidgin/About/contributors.json",
		G_RESOURCE_LOOKUP_FLAGS_NONE,
		NULL
	);

	/* create our parser */
	parser = json_parser_new();

	if(!json_parser_load_from_stream(parser, istream, NULL, &error)) {
		g_critical("%s", error->message);
	}

	root_node = json_parser_get_root(parser);
	root_object = json_node_get_object(root_node);

	credits = json_object_get_array_member(root_object, "credits");
	sections = json_array_get_elements(credits);

	for(l = sections; l; l = l->next) {
		GtkTreeIter section_iter;
		GList *ll = NULL, *people = NULL;
		JsonObject *section = json_node_get_object(l->data);
		JsonArray *people_array = NULL;
		gchar *markup = NULL;

		markup = g_strdup_printf(
			"<span font_weight=\"bold\" font_size=\"large\">%s</span>",
			json_object_get_string_member(section, "title")
		);

		gtk_tree_store_append(about->priv->credits_store, &section_iter, NULL);
		gtk_tree_store_set(
			about->priv->credits_store,
			&section_iter,
			0, markup,
			1, 0.5f,
			-1
		);

		g_free(markup);

		people_array = json_object_get_array_member(section, "people");
		people = json_array_get_elements(people_array);

		for(ll = people; ll; ll = ll->next) {
			GtkTreeIter person_iter;
			gchar *markup = g_strdup(json_node_get_string(ll->data));

			gtk_tree_store_append(about->priv->credits_store, &person_iter, &section_iter);
			gtk_tree_store_set(
				about->priv->credits_store,
				&person_iter,
				0, markup,
				1, 0.5f,
				-1
			);

			g_free(markup);
		}

		g_list_free(people);
	}

	g_list_free(sections);

	/* clean up */
	g_object_unref(G_OBJECT(parser));

	g_input_stream_close(istream, NULL, NULL);
}

static void
_pidgin_about_dialog_add_build_args(
	PidginAboutDialog *about,
	const gchar *title,
	const gchar *build_args
) {
	GtkTreeIter section, value;
	gchar **splits = NULL;
	gchar *markup = NULL;
	gint idx = 0;

	markup = g_strdup_printf("<span font-weight=\"bold\">%s</span>", title);
	gtk_tree_store_append(about->priv->build_info_store, &section, NULL);
	gtk_tree_store_set(
		about->priv->build_info_store,
		&section,
		0, markup,
		-1
	);
	g_free(markup);

	/* now walk through the arguments and add them */
	splits = g_strsplit(build_args, " ", -1);
	for(idx = 0; splits[idx]; idx++) {
		gchar **value_split = g_strsplit(splits[idx], "=", 2);

		gtk_tree_store_append(about->priv->build_info_store, &value, &section);
		gtk_tree_store_set(
			about->priv->build_info_store,
			&value,
			0, value_split[0],
			1, value_split[1],
			-1
		);

		g_strfreev(value_split);
	}

	g_strfreev(splits);
}

static void
_pidgin_about_dialog_build_info_add_version(
	GtkTreeStore *store,
	GtkTreeIter *section,
	const gchar *title,
	guint major,
	guint minor,
	guint micro
) {
	GtkTreeIter item;
	gchar *version = g_strdup_printf("%u.%u.%u", major, minor, micro);

	gtk_tree_store_append(store, &item, section);
	gtk_tree_store_set(
		store, &item,
		0, title,
		1, version,
		-1
	);
	g_free(version);
}

static void
_pidgin_about_dialog_load_build_info(PidginAboutDialog *about) {
	GtkTreeIter section;
	gchar *markup = NULL;

	/* create the section */
	markup = g_strdup_printf(
		"<span font-weight=\"bold\">%s</span>",
		_("Build Information")
	);
	gtk_tree_store_append(about->priv->build_info_store, &section, NULL);
	gtk_tree_store_set(
		about->priv->build_info_store,
		&section,
		0, markup,
		-1
	);
	g_free(markup);

	/* add the purple version */
	_pidgin_about_dialog_build_info_add_version(
		about->priv->build_info_store,
		&section,
		_("Purple Version"),
		PURPLE_MAJOR_VERSION,
		PURPLE_MINOR_VERSION,
		PURPLE_MICRO_VERSION
	);

	/* add the glib version */
	_pidgin_about_dialog_build_info_add_version(
		about->priv->build_info_store,
		&section,
		_("GLib Version"),
		GLIB_MAJOR_VERSION,
		GLIB_MINOR_VERSION,
		GLIB_MICRO_VERSION
	);

	/* add the gtk version */
	_pidgin_about_dialog_build_info_add_version(
		about->priv->build_info_store,
		&section,
		_("Gtk+ Version"),
		GTK_MAJOR_VERSION,
		GTK_MINOR_VERSION,
		GTK_MICRO_VERSION
	);
}

static void
_pidgin_about_dialog_load_runtime_info(PidginAboutDialog *about) {
	GtkTreeIter section;
	gchar *markup = NULL;

	/* create the section */
	markup = g_strdup_printf(
		"<span font-weight=\"bold\">%s</span>",
		_("Runtime Information")
	);
	gtk_tree_store_append(about->priv->build_info_store, &section, NULL);
	gtk_tree_store_set(
		about->priv->build_info_store,
		&section,
		0, markup,
		-1
	);
	g_free(markup);

	/* add the purple version */
	_pidgin_about_dialog_build_info_add_version(
		about->priv->build_info_store,
		&section,
		_("Purple Version"),
		purple_major_version,
		purple_minor_version,
		purple_micro_version
	);

	/* add the glib version */
	_pidgin_about_dialog_build_info_add_version(
		about->priv->build_info_store,
		&section,
		_("GLib Version"),
		glib_major_version,
		glib_minor_version,
		glib_micro_version
	);

	/* add the gtk version */
	_pidgin_about_dialog_build_info_add_version(
		about->priv->build_info_store,
		&section,
		_("Gtk+ Version"),
		gtk_major_version,
		gtk_minor_version,
		gtk_micro_version
	);
}

static void
_pidgin_about_dialog_load_build_configuration(PidginAboutDialog *about) {
#ifdef MESON_ARGS
	_pidgin_about_dialog_add_build_args(about, "Meson Arguments", MESON_ARGS);
#endif /* MESON_ARGS */
#ifdef CONFIG_ARGS
	_pidgin_about_dialog_add_build_args(about, "Configure Arguments", CONFIG_ARGS);
#endif /* CONFIG_ARGS */

	_pidgin_about_dialog_load_build_info(about);
	_pidgin_about_dialog_load_runtime_info(about);
}

/******************************************************************************
 * Callbacks
 *****************************************************************************/
static void
_pidgin_about_dialog_toggle_credits(GtkToggleButton *b, gpointer d) {
	PidginAboutDialog *about = d;
	gboolean show = FALSE;

	if(about->priv->switching_pages)
		return;

	show = gtk_toggle_button_get_active(b);

	_pidgin_about_dialog_switch_page(d, show ? "credits" : "main");
}

static void
_pidgin_about_dialog_toggle_build_info(GtkToggleButton *b, gpointer d) {
	PidginAboutDialog *about = d;
	gboolean show = FALSE;

	if(about->priv->switching_pages)
		return;

	show = gtk_toggle_button_get_active(b);

	_pidgin_about_dialog_switch_page(d, show ? "build-info" : "main");
}

/******************************************************************************
 * GObject Stuff
 *****************************************************************************/
G_DEFINE_TYPE_WITH_PRIVATE(PidginAboutDialog, pidgin_about_dialog, GTK_TYPE_DIALOG);

static void
pidgin_about_dialog_class_init(PidginAboutDialogClass *klass) {
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	gtk_widget_class_set_template_from_resource(
		widget_class,
		"/im/pidgin/Pidgin/About/about.ui"
	);

	gtk_widget_class_bind_template_child_private(widget_class, PidginAboutDialog, stack);

	gtk_widget_class_bind_template_child_private(widget_class, PidginAboutDialog, credits_button);
	gtk_widget_class_bind_template_child_private(widget_class, PidginAboutDialog, credits_page);
	gtk_widget_class_bind_template_child_private(widget_class, PidginAboutDialog, credits_store);
	gtk_widget_class_bind_template_child_private(widget_class, PidginAboutDialog, credits_treeview);

	gtk_widget_class_bind_template_child_private(widget_class, PidginAboutDialog, build_info_button);
	gtk_widget_class_bind_template_child_private(widget_class, PidginAboutDialog, build_info_page);
	gtk_widget_class_bind_template_child_private(widget_class, PidginAboutDialog, build_info_store);
	gtk_widget_class_bind_template_child_private(widget_class, PidginAboutDialog, build_info_treeview);
}

static void
pidgin_about_dialog_init(PidginAboutDialog *about) {
	about->priv = pidgin_about_dialog_get_instance_private(about);

	about->priv->switching_pages = FALSE;

	gtk_widget_init_template(GTK_WIDGET(about));

	/* setup the credits stuff */
	g_signal_connect(
		about->priv->credits_button,
		"toggled",
		G_CALLBACK(_pidgin_about_dialog_toggle_credits),
		about
	);

	_pidgin_about_dialog_load_contributors(about);
	gtk_tree_view_expand_all(GTK_TREE_VIEW(about->priv->credits_treeview));

	/* setup the build info page */
	g_signal_connect(
		about->priv->build_info_button,
		"toggled",
		G_CALLBACK(_pidgin_about_dialog_toggle_build_info),
		about
	);

	_pidgin_about_dialog_load_build_configuration(about);
	gtk_tree_view_expand_all(GTK_TREE_VIEW(about->priv->build_info_treeview));
}

GtkWidget *
pidgin_about_dialog_new(void) {
	GtkWidget *about = NULL;

	about = g_object_new(
		PIDGIN_TYPE_ABOUT_DIALOG,
		"title", "About Pidgin",
		NULL
	);

	return about;
}

