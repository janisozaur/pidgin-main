/*
 * @file gtkscrollbook.c GTK+ Status Selection Widget
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

#include "gtkscrollbook.h"


static void gtk_gaim_scroll_book_init (GtkGaimScrollBook *scroll_book);
static void gtk_gaim_scroll_book_class_init (GtkGaimScrollBookClass *klass);

GType
gtk_gaim_scroll_book_get_type (void)
{
	static GType scroll_book_type = 0;

	if (!scroll_book_type)
	{
		static const GTypeInfo scroll_book_info =
		{
			sizeof (GtkGaimScrollBookClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) gtk_gaim_scroll_book_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GtkGaimScrollBook),
			0,
			(GInstanceInitFunc) gtk_gaim_scroll_book_init,
			NULL  /* value_table */
		};

		scroll_book_type = g_type_register_static(GTK_TYPE_VBOX,
							 "GtkGaimScrollBook",
							 &scroll_book_info,
							 0);
	}

	return scroll_book_type;
}

static void
scroll_left_cb(GtkGaimScrollBook *scroll_book)
{
	int index;
	index = gtk_notebook_get_current_page(GTK_NOTEBOOK(scroll_book->notebook));

	if (index > 0)
		gtk_notebook_set_current_page(GTK_NOTEBOOK(scroll_book->notebook), index - 1);
}

static void
scroll_right_cb(GtkGaimScrollBook *scroll_book)
{
	int index, count;
	index = gtk_notebook_get_current_page(GTK_NOTEBOOK(scroll_book->notebook));
#if GTK_CHECK_VERSION(2,2,0)
	count = gtk_notebook_get_n_pages(GTK_NOTEBOOK(scroll_book->notebook));
#else
	count = g_list_length(GTK_NOTEBOOK(scroll_book->notebook)->children);
#endif	
	
	if (index + 1 < count)
		gtk_notebook_set_current_page(GTK_NOTEBOOK(scroll_book->notebook), index + 1);
}

static void
refresh_scroll_box(GtkGaimScrollBook *scroll_book, int index, int count)
{
	char *label;

	gtk_widget_show_all(scroll_book);
	if (count <= 1)
		gtk_widget_hide(GTK_WIDGET(scroll_book->hbox));
	else
		gtk_widget_show_all(GTK_WIDGET(scroll_book->hbox));
		
	
	label = g_strdup_printf("<span size='smaller' weight='bold'>(%d/%d)</span>", index+1, count);
	gtk_label_set_markup(GTK_LABEL(scroll_book->label), label);
	g_free(label);				      

	if (index == 0)
		gtk_widget_set_sensitive(scroll_book->left_arrow, FALSE);
	else
		gtk_widget_set_sensitive(scroll_book->left_arrow, TRUE);

	
	if (index +1== count)
		gtk_widget_set_sensitive(scroll_book->right_arrow, FALSE);
	else
		gtk_widget_set_sensitive(scroll_book->right_arrow, TRUE);
}


static void
page_count_change_cb(GtkNotebook *notebook, GtkNotebookPage *page, guint page_num, GtkGaimScrollBook *scroll_book)
{
	int index = gtk_notebook_get_current_page(GTK_NOTEBOOK(scroll_book->notebook));
#if GTK_CHECK_VERSION(2,2,0)
	int count = gtk_notebook_get_n_pages(GTK_NOTEBOOK(scroll_book->notebook));
#else
	count = g_list_length(GTK_NOTEBOOK(scroll_book->notebook)->children);
#endif
	refresh_scroll_box(scroll_book, index, count);
	
}

static void
switch_page_cb(GtkNotebook *notebook, GtkNotebookPage *page, guint page_num, GtkGaimScrollBook *scroll_book)
{
#if GTK_CHECK_VERSION(2,2,0)
	int count = gtk_notebook_get_n_pages(GTK_NOTEBOOK(scroll_book->notebook));
#else
	count = g_list_length(GTK_NOTEBOOK(scroll_book->notebook)->children);
#endif
	refresh_scroll_box(scroll_book, page_num, count);
}

static void
gtk_gaim_scroll_book_add(GtkContainer *container, GtkWidget *widget)
{
	gtk_widget_show(widget);
	gtk_notebook_append_page(GTK_NOTEBOOK(GTK_GAIM_SCROLL_BOOK(container)->notebook), widget, NULL);
}

static void
gtk_gaim_scroll_book_class_init (GtkGaimScrollBookClass *klass)
{
	GtkContainerClass *container_class = (GtkContainerClass*)klass;

	container_class->add = gtk_gaim_scroll_book_add;
	
}

static void
gtk_gaim_scroll_book_init (GtkGaimScrollBook *scroll_book)
{
	GtkWidget *eb;

	scroll_book->hbox = gtk_hbox_new(FALSE, 0);

	eb = gtk_event_box_new();
	gtk_box_pack_end(GTK_BOX(scroll_book->hbox), eb, FALSE, FALSE, 0);
	scroll_book->right_arrow = gtk_arrow_new(GTK_ARROW_RIGHT, GTK_SHADOW_NONE);
	gtk_container_add(GTK_CONTAINER(eb), scroll_book->right_arrow);
	g_signal_connect_swapped(G_OBJECT(eb), "button-press-event", G_CALLBACK(scroll_right_cb), scroll_book);

	scroll_book->label = gtk_label_new(NULL);
	gtk_box_pack_end(GTK_BOX(scroll_book->hbox), scroll_book->label, FALSE, FALSE, 0);

	eb = gtk_event_box_new();
	gtk_box_pack_end(GTK_BOX(scroll_book->hbox), eb, FALSE, FALSE, 0);
	scroll_book->left_arrow = gtk_arrow_new(GTK_ARROW_LEFT, GTK_SHADOW_NONE);
	gtk_container_add(GTK_CONTAINER(eb), scroll_book->left_arrow);
	g_signal_connect_swapped(G_OBJECT(eb), "button-press-event", G_CALLBACK(scroll_left_cb), scroll_book);

	gtk_box_pack_start(GTK_BOX(scroll_book), scroll_book->hbox, FALSE, FALSE, 0);
	
	scroll_book->notebook = gtk_notebook_new();
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(scroll_book->notebook), FALSE);
	gtk_notebook_set_show_border(GTK_NOTEBOOK(scroll_book->notebook), FALSE);
	
	gtk_box_pack_start(GTK_BOX(scroll_book), scroll_book->notebook, TRUE, TRUE, 0);
	
	g_signal_connect(G_OBJECT(scroll_book->notebook), "page-added", G_CALLBACK(page_count_change_cb), scroll_book);
	g_signal_connect(G_OBJECT(scroll_book->notebook), "page-removed", G_CALLBACK(page_count_change_cb), scroll_book);
	g_signal_connect(G_OBJECT(scroll_book->notebook), "switch-page", G_CALLBACK(switch_page_cb), scroll_book);
	gtk_widget_hide(scroll_book->hbox);
}



GtkWidget *
gtk_gaim_scroll_book_new()
{
	return g_object_new(GTK_GAIM_TYPE_SCROLL_BOOK, NULL);
}
