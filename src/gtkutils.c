/**
 * @file gtkutils.h GTK+ utility functions
 * @ingroup gtkui
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "gtkinternal.h"

#ifndef _WIN32
# include <X11/Xlib.h>
#else
# ifdef small
#  undef small
# endif
#endif /*_WIN32*/

#ifdef USE_GTKSPELL
# include <gtkspell/gtkspell.h>
# ifdef _WIN32
#  include "wspell.h"
# endif
#endif

#include <gdk/gdkkeysyms.h>

#include "debug.h"
#include "imgstore.h"
#include "notify.h"
#include "prefs.h"
#include "prpl.h"
#include "signals.h"
#include "util.h"

#include "gtkconv.h"
#include "gtkimhtml.h"
#include "gtkutils.h"
#include "ui.h"

guint accels_save_timer = 0;

static void
url_clicked_cb(GtkWidget *w, const char *uri)
{
	gaim_notify_uri(NULL, uri);
}

void
gaim_setup_imhtml(GtkWidget *imhtml)
{
	g_return_if_fail(imhtml != NULL);
	g_return_if_fail(GTK_IS_IMHTML(imhtml));

	if (!gaim_prefs_get_bool("/gaim/gtk/conversations/show_smileys"))
		gtk_imhtml_show_smileys(GTK_IMHTML(imhtml), FALSE);

	g_signal_connect(G_OBJECT(imhtml), "url_clicked",
					 G_CALLBACK(url_clicked_cb), NULL);

	smiley_themeize(imhtml);
}

void
toggle_sensitive(GtkWidget *widget, GtkWidget *to_toggle)
{
	gboolean sensitivity = GTK_WIDGET_IS_SENSITIVE(to_toggle);

	gtk_widget_set_sensitive(to_toggle, !sensitivity);
}

void
gaim_gtk_set_sensitive_if_input(GtkWidget *entry, GtkWidget *dialog)
{
	const char *text = gtk_entry_get_text(GTK_ENTRY(entry));
	gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), GTK_RESPONSE_OK,
									  (*text != '\0'));
}

static void
gaim_gtk_remove_tags(GaimGtkConversation *gtkconv, const char *tag)
{
	GtkTextIter start, end, m_start, m_end;

	if (gtkconv == NULL || tag == NULL)
		return;

	if (!gtk_text_buffer_get_selection_bounds(gtkconv->entry_buffer,
											  &start, &end))
		return;

	/* FIXMEif (strstr(tag, "<FONT SIZE=")) {
		while ((t = strstr(t, "<FONT SIZE="))) {
			if (((t - s) < finish) && ((t - s) >= start)) {
				gtk_editable_delete_text(GTK_EDITABLE(entry), (t - s),
							 (t - s) + strlen(tag));
				g_free(s);
				s = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
				t = s;
			} else
				t++;
		}
	} else*/ {
		while (gtk_text_iter_forward_search(&start, tag, 0, &m_start,
											&m_end, &end)) {

			gtk_text_buffer_delete(gtkconv->entry_buffer, &m_start, &m_end);
			gtk_text_buffer_get_selection_bounds(gtkconv->entry_buffer,
												 &start, &end);
		}
	}
}

void
gaim_gtk_surround(GaimGtkConversation *gtkconv,
				  const char *pre, const char *post)
{
	GtkTextIter start, end;
	GtkTextMark *mark_start, *mark_end;
	GtkTextBuffer *entry_buffer;

	if (gtkconv == NULL || pre == NULL || post == NULL)
		return;

	entry_buffer = gtkconv->entry_buffer;

	if (gtk_text_buffer_get_selection_bounds(entry_buffer,
											 &start, &end)) {
		gaim_gtk_remove_tags(gtkconv, pre);
		gaim_gtk_remove_tags(gtkconv, post);

		mark_start = gtk_text_buffer_create_mark(entry_buffer, "m1",
												 &start, TRUE);
		mark_end = gtk_text_buffer_create_mark(entry_buffer, "m2",
											   &end, FALSE);
		gtk_text_buffer_insert(entry_buffer, &start, pre, -1);
		gtk_text_buffer_get_selection_bounds(entry_buffer, &start, &end);
		gtk_text_buffer_insert(entry_buffer, &end, post, -1);
		gtk_text_buffer_get_iter_at_mark(entry_buffer, &start, mark_start);
		gtk_text_buffer_move_mark_by_name(entry_buffer, "selection_bound",
										  &start);
	} else {
		gtk_text_buffer_insert(entry_buffer, &start, pre, -1);
		gtk_text_buffer_insert(entry_buffer, &start, post, -1);
		mark_start = gtk_text_buffer_get_insert(entry_buffer);
		gtk_text_buffer_get_iter_at_mark(entry_buffer, &start, mark_start);
		gtk_text_iter_backward_chars(&start, strlen(post));
		gtk_text_buffer_place_cursor(entry_buffer, &start);
	}

	gtk_widget_grab_focus(gtkconv->entry);
}

static gboolean
invert_tags(GtkTextBuffer *buffer, const char *s1, const char *s2,
			gboolean really)
{
	GtkTextIter start1, start2, end1, end2;
	char *b1, *b2;

	if (gtk_text_buffer_get_selection_bounds(buffer, &start1, &end2)) {
		start2 = start1;
		end1 = end2;

		if (!gtk_text_iter_forward_chars(&start2, strlen(s1)))
			return FALSE;

		if (!gtk_text_iter_backward_chars(&end1, strlen(s2)))
			return FALSE;

		b1 = gtk_text_buffer_get_text(buffer, &start1, &start2, FALSE);
		b2 = gtk_text_buffer_get_text(buffer, &end1, &end2, FALSE);

		if (!g_ascii_strncasecmp(b1, s1, strlen(s1)) &&
		    !g_ascii_strncasecmp(b2, s2, strlen(s2))) {

			if (really) {
				GtkTextMark *m_end1, *m_end2;
 
				m_end1= gtk_text_buffer_create_mark(buffer, "m1", &end1, TRUE);
				m_end2= gtk_text_buffer_create_mark(buffer, "m2", &end2, TRUE);

				gtk_text_buffer_delete(buffer, &start1, &start2);
				gtk_text_buffer_get_iter_at_mark(buffer, &end1, m_end1);
				gtk_text_buffer_get_iter_at_mark(buffer, &end2, m_end2);
				gtk_text_buffer_delete(buffer, &end1, &end2);
				gtk_text_buffer_delete_mark(buffer, m_end1);
				gtk_text_buffer_delete_mark(buffer, m_end2);
			}

			g_free(b1);
			g_free(b2);

			return TRUE;
		}

		g_free(b1);
		g_free(b2);
	}

	return FALSE;
}

void
gaim_gtk_advance_past(GaimGtkConversation *gtkconv,
					  const char *pre, const char *post)
{
	GtkTextIter current_pos, start, end;

	if (invert_tags(gtkconv->entry_buffer, pre, post, TRUE))
		return;

	gtk_text_buffer_get_iter_at_mark(gtkconv->entry_buffer, &current_pos,
			gtk_text_buffer_get_insert(gtkconv->entry_buffer));

	if (gtk_text_iter_forward_search(&current_pos, post, 0,
									 &start, &end, NULL))
		gtk_text_buffer_place_cursor(gtkconv->entry_buffer, &end);
	else
		gtk_text_buffer_insert_at_cursor(gtkconv->entry_buffer, post, -1);

	gtk_widget_grab_focus(gtkconv->entry);
}

void
gaim_gtk_set_font_face(GaimGtkConversation *gtkconv,
					   const char *font)
{
	if (gtkconv == NULL || font == NULL)
		return;

	strncpy(gtkconv->fontface,
			(font && *font ? font : DEFAULT_FONT_FACE),
			sizeof(gtkconv->fontface));

	gtkconv->has_font = TRUE;

	gtk_imhtml_toggle_fontface(GTK_IMHTML(gtkconv->entry), gtkconv->fontface);

	gtk_widget_grab_focus(gtkconv->entry);

}

static int
des_save_icon(GtkObject *obj, GdkEvent *e,
			  GaimGtkConversation *gtkconv)
{
	gtk_widget_destroy(gtkconv->u.im->save_icon);
	gtkconv->u.im->save_icon = NULL;

	return TRUE;
}

static void
do_save_icon(GtkObject *obj, GaimConversation *c)
{
	GaimGtkConversation *gtkconv;
	FILE *file;
	const char *f;

	gtkconv = GAIM_GTK_CONVERSATION(c);

	f = gtk_file_selection_get_filename(
		GTK_FILE_SELECTION(gtkconv->u.im->save_icon));

	if (gaim_gtk_check_if_dir(f, GTK_FILE_SELECTION(gtkconv->u.im->save_icon)))
		return;

	if ((file = fopen(f, "w")) != NULL) {
		GaimBuddyIcon *icon = gaim_conv_im_get_icon(GAIM_CONV_IM(c));
		size_t len;
		const void *data = gaim_buddy_icon_get_data(icon, &len);

		if (data)
			fwrite(data, 1, len, file);

		fclose(file);
	} else {
		gaim_notify_error(NULL, NULL,
						  _("Can't save icon file to disk."), NULL);
	}

	gtk_widget_destroy(gtkconv->u.im->save_icon);
	gtkconv->u.im->save_icon = NULL;
}

static void
cancel_save_icon(GtkObject *obj, GaimGtkConversation *gtkconv)
{
	gtk_widget_destroy(gtkconv->u.im->save_icon);
	gtkconv->u.im->save_icon = NULL;
}


void
gaim_gtk_save_icon_dialog(GtkObject *obj, GaimConversation *conv)
{
	GaimGtkConversation *gtkconv;
	char buf[BUF_LEN];

	if (conv == NULL || gaim_conversation_get_type(conv) != GAIM_CONV_IM)
		return;

	if (!GAIM_IS_GTK_CONVERSATION(conv))
		return;

	gtkconv = GAIM_GTK_CONVERSATION(conv);

	if (gtkconv->u.im->save_icon != NULL)
	{
		gdk_window_raise(gtkconv->u.im->save_icon->window);
		return;
	}

	gtkconv->u.im->save_icon = gtk_file_selection_new(_("Save Icon"));

	gtk_file_selection_hide_fileop_buttons(
		GTK_FILE_SELECTION(gtkconv->u.im->save_icon));

	g_snprintf(buf, BUF_LEN - 1,
			   "%s" G_DIR_SEPARATOR_S "%s.icon",
			   gaim_home_dir(), gaim_conversation_get_name(conv));

	gtk_file_selection_set_filename(
		GTK_FILE_SELECTION(gtkconv->u.im->save_icon), buf);

	g_signal_connect(G_OBJECT(gtkconv->u.im->save_icon), "delete_event",
					 G_CALLBACK(des_save_icon), gtkconv);
	g_signal_connect(G_OBJECT(GTK_FILE_SELECTION(gtkconv->u.im->save_icon)->ok_button), "clicked",
					 G_CALLBACK(do_save_icon), conv);
	g_signal_connect(G_OBJECT(GTK_FILE_SELECTION(gtkconv->u.im->save_icon)->cancel_button), "clicked",
					 G_CALLBACK(cancel_save_icon), gtkconv);

	gtk_widget_show(gtkconv->u.im->save_icon);
}

int
gaim_gtk_get_dispstyle(GaimConversationType type)
{
	int dispstyle = 2;
	int value;

	if (type == GAIM_CONV_CHAT) {
		value = gaim_prefs_get_int("/gaim/gtk/conversations/chat/button_type");

		switch (value) {
			case GAIM_BUTTON_TEXT:  dispstyle = 1; break;
			case GAIM_BUTTON_IMAGE: dispstyle = 0; break;
			default:                dispstyle = 2; break; /* both/neither */
		}
	}
	else if (type == GAIM_CONV_IM) {
		value = gaim_prefs_get_int("/gaim/gtk/conversations/im/button_type");

		switch (value) {
			case GAIM_BUTTON_TEXT:  dispstyle = 1; break;
			case GAIM_BUTTON_IMAGE: dispstyle = 0; break;
			default:                dispstyle = 2; break; /* both/neither */
		}
	}

	return dispstyle;
}

GtkWidget *
gaim_gtk_change_text(const char *text, GtkWidget *button,
					 const char *stock, GaimConversationType type)
{
	int dispstyle = gaim_gtk_get_dispstyle(type);

	if (button != NULL)
		gtk_widget_destroy(button);

	button = gaim_pixbuf_button_from_stock((dispstyle == 0 ? NULL : text),
										   (dispstyle == 1 ? NULL : stock),
										   GAIM_BUTTON_VERTICAL);

	gtk_widget_show(button);

	return button;
}

void
gaim_gtk_toggle_sensitive(GtkWidget *widget, GtkWidget *to_toggle)
{
	gboolean sensitivity;

	if (to_toggle == NULL)
		return;

	sensitivity = GTK_WIDGET_IS_SENSITIVE(to_toggle);

	gtk_widget_set_sensitive(to_toggle, !sensitivity);
}

void
gtk_toggle_sensitive_array(GtkWidget *w, GPtrArray *data)
{
	gboolean sensitivity;
	gpointer element;
	int i;

	for (i=0; i < data->len; i++) {
		element = g_ptr_array_index(data,i);
		if (element == NULL)
			continue;

		sensitivity = GTK_WIDGET_IS_SENSITIVE(element);

		gtk_widget_set_sensitive(element, !sensitivity);	
	}
}

void gaim_separator(GtkWidget *menu)
{
	GtkWidget *menuitem;

	menuitem = gtk_separator_menu_item_new();
	gtk_widget_show(menuitem);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
}

GtkWidget *gaim_new_item(GtkWidget *menu, const char *str)
{
	GtkWidget *menuitem;
	GtkWidget *label;

	menuitem = gtk_menu_item_new();
	if (menu)
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	gtk_widget_show(menuitem);

	label = gtk_label_new(str);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_label_set_pattern(GTK_LABEL(label), "_");
	gtk_container_add(GTK_CONTAINER(menuitem), label);
	gtk_widget_show(label);
/* FIXME: Go back and fix this 
	gtk_widget_add_accelerator(menuitem, "activate", accel, str[0],
				   GDK_MOD1_MASK, GTK_ACCEL_LOCKED);
*/
	gaim_set_accessible_label (menuitem, label);
	return menuitem;
}

GtkWidget *gaim_new_check_item(GtkWidget *menu, const char *str,
		GtkSignalFunc sf, gpointer data, gboolean checked)
{
	GtkWidget *menuitem;
	menuitem = gtk_check_menu_item_new_with_mnemonic(str);

	if (menu)
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), checked);

	if (sf)
		g_signal_connect(G_OBJECT(menuitem), "activate", sf, data);

	gtk_widget_show_all(menuitem);

	return menuitem;
}

GtkWidget *
gaim_pixbuf_toolbar_button_from_stock(const char *icon)
{
	GtkWidget *button, *image,  *bbox;

	button = gtk_toggle_button_new();
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);

	bbox = gtk_vbox_new(FALSE, 0);

	gtk_container_add (GTK_CONTAINER(button), bbox);

	image = gtk_image_new_from_stock(icon, GTK_ICON_SIZE_MENU);
	gtk_box_pack_start(GTK_BOX(bbox), image, FALSE, FALSE, 0);

	gtk_widget_show_all(bbox);

	return button;
}

GtkWidget *
gaim_pixbuf_button_from_stock(const char *text, const char *icon,
							  GaimButtonOrientation style)
{
	GtkWidget *button, *image, *label, *bbox, *ibox, *lbox;

	button = gtk_button_new();

	if (style == GAIM_BUTTON_HORIZONTAL) {
			bbox = gtk_hbox_new(FALSE, 5);
			ibox = gtk_hbox_new(FALSE, 0);
			lbox = gtk_hbox_new(FALSE, 0);
	}
	else {
			bbox = gtk_vbox_new(FALSE, 5);
			ibox = gtk_vbox_new(FALSE, 0);
			lbox = gtk_vbox_new(FALSE, 0);
	}

	gtk_container_add (GTK_CONTAINER(button), bbox);

	gtk_box_pack_start_defaults(GTK_BOX(bbox), ibox);
	gtk_box_pack_start_defaults(GTK_BOX(bbox), lbox);

	if (icon) {
		image = gtk_image_new_from_stock(icon, GTK_ICON_SIZE_BUTTON);
		gtk_box_pack_end(GTK_BOX(ibox), image, FALSE, FALSE, 0);
	}

	if (text) {
		label = gtk_label_new(NULL);
		gtk_label_set_text_with_mnemonic(GTK_LABEL(label), text);
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), button);
		gtk_box_pack_start(GTK_BOX(lbox), label, FALSE, FALSE, 0);
		gaim_set_accessible_label (button, label);
	}

	gtk_widget_show_all(bbox);

	return button;
}


GtkWidget *gaim_new_item_from_stock(GtkWidget *menu, const char *str, const char *icon, GtkSignalFunc sf, gpointer data, guint accel_key, guint accel_mods, char *mod)
{
	GtkWidget *menuitem;
	/*
	GtkWidget *hbox;
	GtkWidget *label;
	*/
	GtkWidget *image;

	if (icon == NULL)
		menuitem = gtk_menu_item_new_with_mnemonic(str);
	else
		menuitem = gtk_image_menu_item_new_with_mnemonic(str);

	if (menu)
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	if (sf)
		g_signal_connect(G_OBJECT(menuitem), "activate", sf, data);

	if (icon != NULL) {
		image = gtk_image_new_from_stock(icon, GTK_ICON_SIZE_MENU);
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem), image);
	}
/* FIXME: this isn't right
	if (mod) {
		label = gtk_label_new(mod);
		gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 2);
		gtk_widget_show(label);
	}
*/
/*
	if (accel_key) {
		gtk_widget_add_accelerator(menuitem, "activate", accel, accel_key,
					   accel_mods, GTK_ACCEL_LOCKED);
	}
*/

	gtk_widget_show_all(menuitem);

	return menuitem;
}

GtkWidget *
gaim_gtk_make_frame(GtkWidget *parent, const char *title)
{
	GtkWidget *vbox, *label, *hbox;
	char labeltitle[256];

	vbox = gtk_vbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(parent), vbox, FALSE, FALSE, 0);
	gtk_widget_show(vbox);

	label = gtk_label_new(NULL);
	g_snprintf(labeltitle, sizeof(labeltitle),
			   "<span weight=\"bold\">%s</span>", title);

	gtk_label_set_markup(GTK_LABEL(label), labeltitle);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);
	gaim_set_accessible_label (vbox, label);

	hbox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new("    ");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	vbox = gtk_vbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);
	gtk_widget_show(vbox);

	return vbox;
}

static void
protocol_menu_cb(GtkWidget *optmenu, GCallback cb)
{
	GtkWidget *menu;
	GtkWidget *item;
	const char *protocol;
	gpointer user_data;

	menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(optmenu));
	item = gtk_menu_get_active(GTK_MENU(menu));

	protocol = g_object_get_data(G_OBJECT(item), "protocol");
	user_data = (g_object_get_data(G_OBJECT(optmenu), "user_data"));

	if (cb != NULL)
		((void (*)(GtkWidget *, const char *, gpointer))cb)(item, protocol,
															user_data);
}

GtkWidget *
gaim_gtk_protocol_option_menu_new(const char *id, GCallback cb,
								  gpointer user_data)
{
	GaimPluginProtocolInfo *prpl_info;
	GaimPlugin *plugin;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *optmenu;
	GtkWidget *menu;
	GtkWidget *item;
	GtkWidget *image;
	GdkPixbuf *pixbuf;
	GdkPixbuf *scale;
	GList *p;
	GtkSizeGroup *sg;
	char *filename;
	const char *proto_name;
	char buf[256];
	int i, selected_index = -1;

	optmenu = gtk_option_menu_new();
	gtk_widget_show(optmenu);

	g_object_set_data(G_OBJECT(optmenu), "user_data", user_data);

	menu = gtk_menu_new();
	gtk_widget_show(menu);

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	for (p = gaim_plugins_get_protocols(), i = 0;
		 p != NULL;
		 p = p->next, i++) {

		plugin = (GaimPlugin *)p->data;
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(plugin);

		/* Create the item. */
		item = gtk_menu_item_new();

		/* Create the hbox. */
		hbox = gtk_hbox_new(FALSE, 4);
		gtk_container_add(GTK_CONTAINER(item), hbox);
		gtk_widget_show(hbox);

		/* Load the image. */
		proto_name = prpl_info->list_icon(NULL, NULL);
		g_snprintf(buf, sizeof(buf), "%s.png", proto_name);

		filename = g_build_filename(DATADIR, "pixmaps", "gaim", "status",
									"default", buf, NULL);
		pixbuf = gdk_pixbuf_new_from_file(filename, NULL);
		g_free(filename);

		if (pixbuf != NULL) {
			/* Scale and insert the image */
			scale = gdk_pixbuf_scale_simple(pixbuf, 16, 16,
											GDK_INTERP_BILINEAR);
			image = gtk_image_new_from_pixbuf(scale);

			g_object_unref(G_OBJECT(pixbuf));
			g_object_unref(G_OBJECT(scale));
		}
		else
			image = gtk_image_new();

		gtk_size_group_add_widget(sg, image);

		gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);
		gtk_widget_show(image);

		/* Create the label. */
		label = gtk_label_new(plugin->info->name);
		gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
		gtk_widget_show(label);

		g_object_set_data(G_OBJECT(item), "protocol", plugin->info->id);

		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		gtk_widget_show(item);
		gaim_set_accessible_label (item, label);

		if (!strcmp(plugin->info->id, id))
			selected_index = i;
	}

	gtk_option_menu_set_menu(GTK_OPTION_MENU(optmenu), menu);

	if (selected_index != -1)
		gtk_option_menu_set_history(GTK_OPTION_MENU(optmenu), selected_index);

	g_signal_connect(G_OBJECT(optmenu), "changed",
					 G_CALLBACK(protocol_menu_cb), cb);

	g_object_unref(sg);

	return optmenu;
}

static void
account_menu_cb(GtkWidget *optmenu, GCallback cb)
{
	GtkWidget *menu;
	GtkWidget *item;
	GaimAccount *account;
	gpointer user_data;

	menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(optmenu));
	item = gtk_menu_get_active(GTK_MENU(menu));

	account   = g_object_get_data(G_OBJECT(item),    "account");
	user_data = g_object_get_data(G_OBJECT(optmenu), "user_data");

	if (cb != NULL)
		((void (*)(GtkWidget *, GaimAccount *, gpointer))cb)(item, account,
															 user_data);
}

static void
create_account_menu(GtkWidget *optmenu, GaimAccount *default_account,
					GaimCheckAccountFunc check_account_func, gboolean show_all)
{
	GaimAccount *account;
	GtkWidget *menu;
	GtkWidget *item;
	GtkWidget *image;
	GtkWidget *hbox;
	GtkWidget *label;
	GdkPixbuf *pixbuf;
	GdkPixbuf *scale;
	GList *list;
	GList *p;
	GtkSizeGroup *sg;
	char *filename;
	const char *proto_name;
	char buf[256];
	int i, selected_index = -1;

	if (show_all)
		list = gaim_accounts_get_all();
	else
		list = gaim_connections_get_all();

	menu = gtk_menu_new();
	gtk_widget_show(menu);

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	for (p = list, i = 0; p != NULL; p = p->next, i++) {
		GaimPluginProtocolInfo *prpl_info = NULL;
		GaimPlugin *plugin;

		if (show_all)
			account = (GaimAccount *)p->data;
		else {
			GaimConnection *gc = (GaimConnection *)p->data;

			account = gaim_connection_get_account(gc);
		}

		if (check_account_func && !check_account_func(account))
			continue;

		plugin = gaim_find_prpl(gaim_account_get_protocol_id(account));

		if (plugin != NULL)
			prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(plugin);

		/* Create the item. */
		item = gtk_menu_item_new();

		/* Create the hbox. */
		hbox = gtk_hbox_new(FALSE, 4);
		gtk_container_add(GTK_CONTAINER(item), hbox);
		gtk_widget_show(hbox);

		/* Load the image. */
		if (prpl_info != NULL) {
			proto_name = prpl_info->list_icon(account, NULL);
			g_snprintf(buf, sizeof(buf), "%s.png", proto_name);

			filename = g_build_filename(DATADIR, "pixmaps", "gaim", "status",
										"default", buf, NULL);
			pixbuf = gdk_pixbuf_new_from_file(filename, NULL);
			g_free(filename);

			if (pixbuf != NULL) {
				/* Scale and insert the image */
				scale = gdk_pixbuf_scale_simple(pixbuf, 16, 16,
												GDK_INTERP_BILINEAR);
				image = gtk_image_new_from_pixbuf(scale);

				g_object_unref(G_OBJECT(pixbuf));
				g_object_unref(G_OBJECT(scale));
			}
			else
				image = gtk_image_new();
		}
		else
			image = gtk_image_new();

		gtk_size_group_add_widget(sg, image);

		gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);
		gtk_widget_show(image);

		g_snprintf(buf, sizeof(buf), "%s (%s)",
				   gaim_account_get_username(account), 
				   (plugin != NULL) ? plugin->info->name : _("Unknown"));

		/* Create the label. */
		label = gtk_label_new(buf);
		gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
		gtk_widget_show(label);

		g_object_set_data(G_OBJECT(item), "account", account);

		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		gtk_widget_show(item);
		gaim_set_accessible_label (item, label);

		if (default_account != NULL && account == default_account)
			selected_index = i;
	}

	g_object_unref(sg);

	gtk_option_menu_set_menu(GTK_OPTION_MENU(optmenu), menu);

	/* Set the place we should be at. */
	if (selected_index != -1)
		gtk_option_menu_set_history(GTK_OPTION_MENU(optmenu), selected_index);
}

static void
regenerate_account_menu(GtkWidget *optmenu)
{
	GtkWidget *menu;
	GtkWidget *item;
	gboolean show_all;
	GaimAccount *account;
	GaimCheckAccountFunc check_account_func;

	menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(optmenu));
	item = gtk_menu_get_active(GTK_MENU(menu));
	account = g_object_get_data(G_OBJECT(item), "account");

	show_all = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(optmenu),
												 "show_all"));

	check_account_func = g_object_get_data(G_OBJECT(optmenu),
										   "check_account_func");

	gtk_option_menu_remove_menu(GTK_OPTION_MENU(optmenu));

	create_account_menu(optmenu, account, check_account_func, show_all);
}

static void
account_menu_sign_on_off_cb(GaimConnection *gc, GtkWidget *optmenu)
{
	regenerate_account_menu(optmenu);
}

static void
account_menu_added_removed_cb(GaimAccount *account, GtkWidget *optmenu)
{
	regenerate_account_menu(optmenu);
}

static gboolean
account_menu_destroyed_cb(GtkWidget *optmenu, GdkEvent *event,
						  void *user_data)
{
	gaim_signals_disconnect_by_handle(optmenu);

	return FALSE;
}

GtkWidget *
gaim_gtk_account_option_menu_new(GaimAccount *default_account,
								 gboolean show_all, GCallback cb,
								 GaimCheckAccountFunc check_account_func,
								 gpointer user_data)
{
	GtkWidget *optmenu;

	/* Create the option menu */
	optmenu = gtk_option_menu_new();
	gtk_widget_show(optmenu);

	g_signal_connect(G_OBJECT(optmenu), "destroy",
					 G_CALLBACK(account_menu_destroyed_cb), NULL);

	/* Register the gaim sign on/off event callbacks. */
	gaim_signal_connect(gaim_connections_get_handle(), "signed-on",
						optmenu, GAIM_CALLBACK(account_menu_sign_on_off_cb),
						optmenu);
	gaim_signal_connect(gaim_connections_get_handle(), "signed-off",
						optmenu, GAIM_CALLBACK(account_menu_sign_on_off_cb),
						optmenu);
	gaim_signal_connect(gaim_accounts_get_handle(), "account-added",
						optmenu, GAIM_CALLBACK(account_menu_added_removed_cb),
						optmenu);
	gaim_signal_connect(gaim_accounts_get_handle(), "account-removed",
						optmenu, GAIM_CALLBACK(account_menu_added_removed_cb),
						optmenu);

	/* Set some data. */
	g_object_set_data(G_OBJECT(optmenu), "user_data", user_data);
	g_object_set_data(G_OBJECT(optmenu), "show_all", GINT_TO_POINTER(show_all));
	g_object_set_data(G_OBJECT(optmenu), "check_account_func",
					  check_account_func);

	/* Create and set the actual menu. */
	create_account_menu(optmenu, default_account, check_account_func, show_all);

	/* And now the last callback. */
	g_signal_connect(G_OBJECT(optmenu), "changed",
					 G_CALLBACK(account_menu_cb), cb);

	return optmenu;
}

gboolean
gaim_gtk_check_if_dir(const char *path, GtkFileSelection *filesel)
{
	char *dirname;

	if (g_file_test(path, G_FILE_TEST_IS_DIR)) {
		/* append a / if needed */
		if (path[strlen(path) - 1] != '/') {
			dirname = g_strconcat(path, "/", NULL);
		} else {
			dirname = g_strdup(path);
		}
		gtk_file_selection_set_filename(filesel, dirname);
		g_free(dirname);
		return TRUE;
	}

	return FALSE;
}

char *stylize(const gchar *text, int length)
{
	gchar *buf;
	char *tmp = g_malloc(length);

	buf = g_malloc(length);
	g_snprintf(buf, length, "%s", text);

	if (gaim_prefs_get_bool("/gaim/gtk/conversations/send_bold")) {
		g_snprintf(tmp, length, "<B>%s</B>", buf);
		strcpy(buf, tmp);
	}

	if (gaim_prefs_get_bool("/gaim/gtk/conversations/send_italic")) {
		g_snprintf(tmp, length, "<I>%s</I>", buf);
		strcpy(buf, tmp);
	}

	if (gaim_prefs_get_bool("/gaim/gtk/conversations/send_underline")) {
		g_snprintf(tmp, length, "<U>%s</U>", buf);
		strcpy(buf, tmp);
	}

	if (gaim_prefs_get_bool("/gaim/gtk/conversations/send_strikethrough")) {
		g_snprintf(tmp, length, "<S>%s</S>", buf);
		strcpy(buf, tmp);
	}

	if (gaim_prefs_get_bool("/gaim/gtk/conversations/use_custom_font")) {
		const char *fontface;

		fontface = gaim_prefs_get_string("/gaim/gtk/conversations/font_face");

		g_snprintf(tmp, length, "<FONT FACE=\"%s\">%s</FONT>", fontface, buf);
		strcpy(buf, tmp);
	}

	if (gaim_prefs_get_bool("/gaim/gtk/conversations/use_custom_size")) {
		int fontsize = gaim_prefs_get_int("/gaim/gtk/conversations/font_size");

		g_snprintf(tmp, length, "<FONT SIZE=\"%d\">%s</FONT>", fontsize, buf);
		strcpy(buf, tmp);
	}

	if (gaim_prefs_get_bool("/gaim/gtk/conversations/use_custom_fgcolor")) {
		GdkColor fgcolor;

		gdk_color_parse(
			gaim_prefs_get_string("/gaim/gtk/conversations/fgcolor"),
			&fgcolor);

		g_snprintf(tmp, length, "<FONT COLOR=\"#%02X%02X%02X\">%s</FONT>",
				   fgcolor.red/256, fgcolor.green/256, fgcolor.blue/256, buf);
		strcpy(buf, tmp);
	}

	if (gaim_prefs_get_bool("/gaim/gtk/conversations/use_custom_bgcolor")) {
		GdkColor bgcolor;

		gdk_color_parse(
			gaim_prefs_get_string("/gaim/gtk/conversations/bgcolor"),
			&bgcolor);

		g_snprintf(tmp, length, "<BODY BGCOLOR=\"#%02X%02X%02X\">%s</BODY>",
				   bgcolor.red/256, bgcolor.green/256, bgcolor.blue/256, buf);
		strcpy(buf, tmp);
	}

	g_free(tmp);
	return buf;
}

void
gaim_gtk_find_images(const char *message, GSList **list)
{
	GData *attribs;
	const char *tmp, *start, *end;

	g_return_if_fail(message != NULL);
	g_return_if_fail(   list != NULL);

	tmp = message;
	while (gaim_markup_find_tag("img", tmp, &start, &end, &attribs)) {
		GaimStoredImage *image = NULL;
		GdkPixbufLoader *loader = NULL;
		GdkPixbuf *pixbuf = NULL;
		GError *error = NULL;
		char *id = g_datalist_get_data(&attribs, "id");

		tmp = end + 1;

		if (id)
			image = gaim_imgstore_get(atoi(id));

		g_datalist_clear(&attribs);

		if (!image) {
			*list = g_slist_append(*list, NULL);
			continue;
		}

		loader = gdk_pixbuf_loader_new();

		if (gdk_pixbuf_loader_write(loader, image->data, image->size, &error)
			&& (pixbuf = gdk_pixbuf_loader_get_pixbuf(loader))) {

			if (image->filename)
				g_object_set_data_full(G_OBJECT(pixbuf), "filename",
					g_strdup(image->filename), g_free);
			g_object_ref(G_OBJECT(pixbuf));
			*list = g_slist_append(*list, pixbuf);
		} else {
			if (error) {
				gaim_debug(GAIM_DEBUG_ERROR, "gtkutils",
						"Failed to make pixbuf from image store: %s\n",
						error->message);
				g_error_free(error);
			} else {
				gaim_debug(GAIM_DEBUG_ERROR, "gtkutils",
						"Failed to make pixbuf from image store: unknown reason\n");
			}
			*list = g_slist_append(*list, NULL);
		}

		gdk_pixbuf_loader_close(loader, NULL);
	}
}

void
gaim_gtk_setup_gtkspell(GtkTextView *textview)
{
#ifdef USE_GTKSPELL
	GError *error = NULL;
	char *locale = NULL;

	g_return_if_fail(textview != NULL);
	g_return_if_fail(GTK_IS_TEXT_VIEW(textview));

	if (gtkspell_new_attach(textview, locale, &error) == NULL && error)
	{
		gaim_debug_warning("gtkspell", "Failed to setup GtkSpell: %s\n",
						   error->message);
		g_error_free(error);
	}
#endif /* USE_GTKSPELL */
}

void
gaim_gtk_save_accels_cb(GtkAccelGroup *accel_group, guint arg1,
														 GdkModifierType arg2, GClosure *arg3,
														 gpointer data)
{
	gaim_debug(GAIM_DEBUG_MISC, "accels", "accel changed, scheduling save.\n");

	if (!accels_save_timer)
		accels_save_timer = g_timeout_add(5000, gaim_gtk_save_accels, NULL);
}

gboolean
gaim_gtk_save_accels(gpointer data)
{
	char *filename = NULL;

	filename = g_build_filename(gaim_user_dir(), G_DIR_SEPARATOR_S,
															"accels", NULL);
	gaim_debug(GAIM_DEBUG_MISC, "accels", "saving accels to %s\n", filename);
	gtk_accel_map_save(filename);
	g_free(filename);

	accels_save_timer = 0;
	return FALSE;
}

void
gaim_gtk_load_accels(gpointer data)
{
	char *filename = NULL;

	filename = g_build_filename(gaim_user_dir(), G_DIR_SEPARATOR_S,
															"accels", NULL);
	gtk_accel_map_load(filename);
	g_free(filename);
}

gboolean
gaim_gtk_parse_x_im_contact(const char *msg, gboolean all_accounts,
							GaimAccount **ret_account, char **ret_protocol,
							char **ret_username, char **ret_alias)
{
	char *protocol = NULL;
	char *username = NULL;
	char *alias    = NULL;
	char *str;
	char *c, *s;
	gboolean valid;

	g_return_val_if_fail(msg          != NULL, FALSE);
	g_return_val_if_fail(ret_protocol != NULL, FALSE);
	g_return_val_if_fail(ret_username != NULL, FALSE);

	s = str = g_strdup(msg);

	while (*s != '\r' && *s != '\n' && *s != '\0')
	{
		char *key, *value;

		key = s;

		/* Grab the key */
		while (*s != '\r' && *s != '\n' && *s != '\0' && *s != ' ')
			s++;

		if (*s == '\r') s++;

		if (*s == '\n')
		{
			s++;
			continue;
		}

		if (*s != '\0') *s++ = '\0';

		/* Clear past any whitespace */
		while (*s != '\0' && *s == ' ')
			s++;

		/* Now let's grab until the end of the line. */
		value = s;

		while (*s != '\r' && *s != '\n' && *s != '\0')
			s++;

		if (*s == '\r') *s++ = '\0';
		if (*s == '\n') *s++ = '\0';

		if ((c = strchr(key, ':')) != NULL)
		{
			if (!g_ascii_strcasecmp(key, "X-IM-Username:"))
				username = g_strdup(value);
			else if (!g_ascii_strcasecmp(key, "X-IM-Protocol:"))
				protocol = g_strdup(value);
			else if (!g_ascii_strcasecmp(key, "X-IM-Alias:"))
				alias = g_strdup(value);
		}
	}

	if (username != NULL && protocol != NULL)
	{
		valid = TRUE;

		*ret_username = username;
		*ret_protocol = protocol;

		if (ret_alias != NULL)
			*ret_alias = alias;

		/* Check for a compatible account. */
		if (ret_account != NULL)
		{
			GList *list;
			GaimAccount *account = NULL;
			GList *l;
			const char *protoname;

			if (all_accounts)
				list = gaim_accounts_get_all();
			else
				list = gaim_connections_get_all();

			for (l = list; l != NULL; l = l->next)
			{
				GaimConnection *gc;
				GaimPluginProtocolInfo *prpl_info = NULL;
				GaimPlugin *plugin;

				if (all_accounts)
				{
					account = (GaimAccount *)l->data;

					plugin = gaim_plugins_find_with_id(
						gaim_account_get_protocol_id(account));

					if (plugin == NULL)
					{
						account = NULL;

						continue;
					}

					prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(plugin);
				}
				else
				{
					gc = (GaimConnection *)l->data;
					account = gaim_connection_get_account(gc);

					prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);
				}

				protoname = prpl_info->list_icon(account, NULL);

				if (!strcmp(protoname, protocol))
					break;

				account = NULL;
			}

			/* Special case for AIM and ICQ */
			if (account == NULL && (!strcmp(protocol, "aim") ||
									!strcmp(protocol, "icq")))
			{
				for (l = list; l != NULL; l = l->next)
				{
					GaimConnection *gc;
					GaimPluginProtocolInfo *prpl_info = NULL;
					GaimPlugin *plugin;

					if (all_accounts)
					{
						account = (GaimAccount *)l->data;

						plugin = gaim_plugins_find_with_id(
							gaim_account_get_protocol_id(account));

						if (plugin == NULL)
						{
							account = NULL;

							continue;
						}

						prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(plugin);
					}
					else
					{
						gc = (GaimConnection *)l->data;
						account = gaim_connection_get_account(gc);

						prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);
					}

					protoname = prpl_info->list_icon(account, NULL);

					if (!strcmp(protoname, "aim") || !strcmp(protoname, "icq"))
						break;

					account = NULL;
				}
			}

			*ret_account = account;
		}
	}
	else
	{
		valid = FALSE;

		if (username != NULL) g_free(username);
		if (protocol != NULL) g_free(protocol);
		if (alias    != NULL) g_free(alias);
	}

	g_free(str);

	return valid;
}

void
gaim_set_accessible_label (GtkWidget *w, GtkWidget *l)
{
	AtkObject *acc, *label;
	AtkObject *rel_obj[1];
	AtkRelationSet *set;
	AtkRelation *relation;
	const gchar *label_text;
	const gchar *existing_name;

	acc = gtk_widget_get_accessible (w);
	label = gtk_widget_get_accessible (l);

	/* If this object has no name, set it's name with the label text */
	existing_name = atk_object_get_name (acc);
	if (!existing_name) {
		label_text = gtk_label_get_text (GTK_LABEL(l));
		if (label_text)
			atk_object_set_name (acc, label_text);
	}

	/* Create the labeled-by relation */
	set = atk_object_ref_relation_set (acc);
	rel_obj[0] = label;
	relation = atk_relation_new (rel_obj, 1, ATK_RELATION_LABELLED_BY);
	atk_relation_set_add (set, relation);
	g_object_unref (relation);

	/* Create the label-for relation */
	set = atk_object_ref_relation_set (label);
	rel_obj[0] = acc;
	relation = atk_relation_new (rel_obj, 1, ATK_RELATION_LABEL_FOR);
	atk_relation_set_add (set, relation);
	g_object_unref (relation);
}
