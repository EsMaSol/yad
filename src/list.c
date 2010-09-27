/*
 * This file is part of YAD.
 *
 * YAD is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * YAD is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with YAD; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Copyright (C) 2008-2010, Victor Ananjevsky <ananasik@gmail.com>
 *
 */

#include <string.h>
#include <stdlib.h>

#include <gdk/gdkkeysyms.h>

#include "yad.h"

static GtkWidget *list_view;

static gboolean
list_activate_cb (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
  if (event->keyval == GDK_Return || event->keyval == GDK_KP_Enter)
    {
      gtk_dialog_response (GTK_DIALOG (data), YAD_RESPONSE_OK);
      return TRUE;
    }
  return FALSE;
}

static void
toggled_cb (GtkCellRendererToggle *cell,
	    gchar *path_str, gpointer data)
{
  gint column;
  gboolean fixed;
  GtkTreeIter iter;
  GtkTreePath *path = gtk_tree_path_new_from_string (path_str);
  GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (list_view));

  column = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (cell), "column"));
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter, column, &fixed, -1);

  fixed ^= 1;

  gtk_list_store_set (GTK_LIST_STORE (model), &iter, column, fixed, -1);

  gtk_tree_path_free (path);
}

static void
cell_edited_cb (GtkCellRendererText * cell,
                const gchar * path_string,
                const gchar * new_text, gpointer data)
{
  gint column;
  GtkTreeIter iter;
  GtkTreePath *path = gtk_tree_path_new_from_string (path_string);;
  GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (list_view));
  YadColumn *col;

  column = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (cell), "column"));
  gtk_tree_model_get_iter (model, &iter, path);
  col = (YadColumn *) g_slist_nth_data (options.list_data.columns, column);

  if (col->type == YAD_COLUMN_NUM)
    gtk_list_store_set (GTK_LIST_STORE (model), &iter, column, g_ascii_strtoll (new_text, NULL, 10), -1);
  else
    gtk_list_store_set (GTK_LIST_STORE (model), &iter, column, new_text, -1);

  gtk_tree_path_free (path);
}

static GtkTreeModel *
create_model (gint n_columns)
{
  GtkListStore *store;
  GType *ctypes;
  gint i;

  ctypes = g_new0 (GType, n_columns);

  if (options.list_data.checkbox)
    {
      YadColumn *col = (YadColumn *) g_slist_nth_data (options.list_data.columns, 0);
      col->type = YAD_COLUMN_CHECK;
    }

  for (i = 0; i < n_columns; i++)
    {
      YadColumn *col = (YadColumn *) g_slist_nth_data (options.list_data.columns, i);

      switch (col->type)
	{
	case YAD_COLUMN_CHECK:
	  ctypes[i] = G_TYPE_BOOLEAN;
	  break;
	case YAD_COLUMN_NUM:
	  ctypes[i] = G_TYPE_INT64;
	  break;
	case YAD_COLUMN_IMAGE:
	  ctypes[i] = GDK_TYPE_PIXBUF;
	  break;
	case YAD_COLUMN_TOOLTIP:
	case YAD_COLUMN_TEXT:
	default:
	  ctypes[i] = G_TYPE_STRING;
	  break;
	}
    }

  store = gtk_list_store_newv (n_columns, ctypes);

  return GTK_TREE_MODEL (store);
}

static void
add_columns (gint n_columns)
{
  gint i;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;

  for (i = 0; i < n_columns; i++)
    {
      YadColumn *col = (YadColumn *) g_slist_nth_data (options.list_data.columns, i);

      switch (col->type)
	{
	case YAD_COLUMN_CHECK:
	  renderer = gtk_cell_renderer_toggle_new ();
	  g_object_set_data (G_OBJECT (renderer), "column", GINT_TO_POINTER (i));
	  g_signal_connect (renderer, "toggled", G_CALLBACK (toggled_cb), NULL);
	  column = gtk_tree_view_column_new_with_attributes
	    (col->name, renderer, "active", i, NULL);
	  break;
	case YAD_COLUMN_IMAGE:
	  renderer = gtk_cell_renderer_pixbuf_new ();
	  column = gtk_tree_view_column_new_with_attributes
	    (col->name, renderer, "pixbuf", i, NULL);
	  break;
	case YAD_COLUMN_NUM:
	case YAD_COLUMN_TEXT:
	case YAD_COLUMN_TOOLTIP:
	default:
	  renderer = gtk_cell_renderer_text_new ();
	  if (options.common_data.editable)
	    {
	      g_object_set (G_OBJECT (renderer), "editable", TRUE, NULL);
	      g_object_set_data (G_OBJECT (renderer), "column", GINT_TO_POINTER (i));
	      g_signal_connect (renderer, "edited", G_CALLBACK (cell_edited_cb), NULL);
	    }
	  column = gtk_tree_view_column_new_with_attributes
	    (col->name, renderer, "text", i, NULL);
	  gtk_tree_view_column_set_sort_column_id (column, i);
	  gtk_tree_view_column_set_resizable  (column, TRUE);
	  break;
	}
      gtk_tree_view_append_column (GTK_TREE_VIEW (list_view), column);
    }
}

static gboolean
handle_stdin (GIOChannel * channel,
              GIOCondition condition, gpointer data)
{
  static GtkTreeIter iter;
  static gint column_count = 0;
  static gint row_count = 0;
  static gboolean first_time = TRUE;
  gint n_columns = GPOINTER_TO_INT (data);
  GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (list_view));

  if (first_time)
    {
      first_time = FALSE;
      gtk_list_store_append (GTK_LIST_STORE (model), &iter);
    }

  if ((condition == G_IO_IN) || (condition == G_IO_IN + G_IO_HUP))
    {
      GError *err = NULL;
      GString *string = g_string_new (NULL);

      while (channel->is_readable != TRUE) ;

      do
        {
	  YadColumn *col;
	  GdkPixbuf *pb;
          gint status;

          do
            {
              status =
                g_io_channel_read_line_string (channel, string, NULL, &err);

              while (gtk_events_pending ())
                gtk_main_iteration ();
            }
          while (status == G_IO_STATUS_AGAIN);
	  strip_new_line (string->str);

          if (status != G_IO_STATUS_NORMAL)
            {
              if (err)
                {
                  g_printerr ("yad_list_handle_stdin(): %s", err->message);
                  g_error_free (err);
		  err = NULL;
                }
              continue;
            }

          if (column_count == n_columns)
            {
              /* We're starting a new row */
              column_count = 0;
              row_count++;
              gtk_list_store_append (GTK_LIST_STORE (model), &iter);
            }

	  col = (YadColumn *) g_slist_nth_data (options.list_data.columns, column_count);

	  switch (col->type)
	    {
	    case YAD_COLUMN_CHECK:
	      if (g_ascii_strcasecmp (string->str, "true") == 0)
		gtk_list_store_set (GTK_LIST_STORE (model), &iter, column_count, TRUE, -1);
	      else
		gtk_list_store_set (GTK_LIST_STORE (model), &iter, column_count, FALSE, -1);
	      break;
	    case YAD_COLUMN_NUM:
	      gtk_list_store_set (GTK_LIST_STORE (model), &iter, column_count,
				  g_ascii_strtoll (string->str, NULL, 10), -1);
	      break;
	    case YAD_COLUMN_IMAGE:
	      pb = get_pixbuf (string->str, YAD_SMALL_ICON);
	      gtk_list_store_set (GTK_LIST_STORE (model), &iter,
				  column_count, pb, -1);
	      g_object_unref (pb);
	      break;
	    case YAD_COLUMN_TEXT:
	    default:
	      gtk_list_store_set (GTK_LIST_STORE (model), &iter,
				  column_count, string->str, -1);
	      break;
	    }

	  column_count++;
        }
      while (g_io_channel_get_buffer_condition (channel) == G_IO_IN);
      g_string_free (string, TRUE);
    }

  if ((condition != G_IO_IN) && (condition != G_IO_IN + G_IO_HUP))
    {
      g_io_channel_shutdown (channel, TRUE, NULL);
      return FALSE;
    }

  return TRUE;
}

static void
fill_data (gint n_columns)
{
  GtkTreeIter iter;
  GtkListStore *model =
    GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (list_view)));

  if (options.extra_data && *options.extra_data)
    {
      gchar **args = options.extra_data;
      gint i = 0;

      while (args[i] != NULL)
	{
	  gint j;

	  gtk_list_store_append (model, &iter);
	  for (j = 0; j < n_columns; j++)
	    {
	      YadColumn *col = (YadColumn *) g_slist_nth_data (options.list_data.columns, j);
	      GdkPixbuf *pb;

	      if (args[i] == NULL)
		break;

	      switch (col->type)
		{
		case YAD_COLUMN_CHECK:
		  if (g_ascii_strcasecmp ((gchar *) args[i], "true") == 0)
		    gtk_list_store_set (model, &iter, j, TRUE, -1);
		  else
		    gtk_list_store_set (model, &iter, j, FALSE, -1);
		  break;
		case YAD_COLUMN_NUM:
		  gtk_list_store_set (GTK_LIST_STORE (model), &iter, j,
				      g_ascii_strtoll (args[i], NULL, 10), -1);
		  break;
		case YAD_COLUMN_IMAGE:
		  pb = get_pixbuf (args[i], YAD_SMALL_ICON);
		  gtk_list_store_set (GTK_LIST_STORE (model), &iter, j, pb, -1);
		  g_object_unref (pb);
		  break;
		case YAD_COLUMN_TEXT:
		default:
		  gtk_list_store_set (GTK_LIST_STORE (model), &iter, j, args[i], -1);
		  break;
		}
	      i++;
	    }
	}
    }
  else
    {
      GIOChannel *channel;

      channel = g_io_channel_unix_new (0);
      g_io_channel_set_encoding (channel, NULL, NULL);
      g_io_channel_set_flags (channel, G_IO_FLAG_NONBLOCK, NULL);
      g_io_add_watch (channel, G_IO_IN | G_IO_HUP,
		      handle_stdin, GINT_TO_POINTER (n_columns));
    }
}

static void
double_click_cb (GtkTreeView *view, GtkTreePath *path,
		 GtkTreeViewColumn *column, gpointer data)
{
  GtkTreeModel *model;
  GtkTreeIter iter;

  model = gtk_tree_view_get_model (view);
  if (gtk_tree_model_get_iter (model, &iter, path))
    {
      gboolean chk;

      gtk_tree_model_get (model, &iter, 0, &chk, -1);
      chk = !chk;
      gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, chk, -1);
    }
}

void
add_row_cb (GtkMenuItem *item, gpointer data)
{
  GtkTreeModel *model;
  GtkTreeIter iter;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (list_view));

  gtk_list_store_append (GTK_LIST_STORE (model), &iter);
}

void
del_row_cb (GtkMenuItem *item, gpointer data)
{
  GtkTreeIter iter;
  GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (list_view));
  GtkTreeSelection *sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (list_view));

  if (gtk_tree_selection_get_selected (sel, NULL, &iter))
    {
      gint i;
      GtkTreePath *path;

      path = gtk_tree_model_get_path (model, &iter);
      i = gtk_tree_path_get_indices (path)[0];
      gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
      gtk_tree_path_free (path);
    }
}

static gboolean
popup_menu_cb (GtkWidget *w, GdkEventButton *ev, gpointer data)
{
  if (ev->button == 3)
    {
      GtkWidget *menu;
      GtkWidget *item;

      menu = gtk_menu_new ();

      item = gtk_image_menu_item_new_with_label (_("Add row"));
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
				     gtk_image_new_from_stock
				     (GTK_STOCK_ADD, GTK_ICON_SIZE_MENU));
      gtk_widget_show (item);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (add_row_cb), NULL);

      item = gtk_image_menu_item_new_with_label (_("Delete row"));
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
				     gtk_image_new_from_stock
				     (GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU));
      gtk_widget_show (item);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (del_row_cb), NULL);

      gtk_widget_show (menu);
      gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL,
		      NULL, ev->button, 0);
    }
  return FALSE;
}

GtkWidget *
list_create_widget (GtkWidget *dlg)
{
  GtkWidget *w;
  GtkTreeModel *model;
  gint i, n_columns;

  n_columns = g_slist_length (options.list_data.columns);

  if (n_columns == 0)
    {
      g_printerr (_("No column titles specified for List dialog.\n"));
      return NULL;
    }

  w = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (w),
				  GTK_POLICY_AUTOMATIC,
				  GTK_POLICY_AUTOMATIC);

  model = create_model (n_columns);

  list_view = gtk_tree_view_new_with_model (model);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (list_view), !options.list_data.no_headers);
  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (list_view), settings.rules_hint);
  gtk_tree_view_set_reorderable (GTK_TREE_VIEW (list_view), options.common_data.editable);
  g_object_unref (model);

  gtk_container_add (GTK_CONTAINER (w), list_view);

  add_columns (n_columns);

  if (options.common_data.multi && !options.list_data.checkbox)
    {
      GtkTreeSelection *sel =
	gtk_tree_view_get_selection (GTK_TREE_VIEW (list_view));
      gtk_tree_selection_set_mode (sel, GTK_SELECTION_MULTIPLE);
    }

  if (options.list_data.checkbox)
    {
      g_signal_connect (G_OBJECT (list_view), "row-activated",
			G_CALLBACK (double_click_cb), NULL);
    }

  if (options.common_data.editable)
    {
      /* add popup menu */
      g_signal_connect_swapped (G_OBJECT (list_view), "button_press_event",
				G_CALLBACK (popup_menu_cb), NULL);
    }

  /* Return submits data */
  g_signal_connect (G_OBJECT (list_view), "key-press-event",
	            G_CALLBACK (list_activate_cb), dlg);

  fill_data (n_columns);

  if (settings.always_selected)
    {
      GtkTreeIter it;
      GtkTreeSelection *sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (list_view));
      GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (list_view));

      gtk_tree_model_get_iter_first (model, &it);
      gtk_tree_selection_select_iter (sel, &it);
    }

  /* add tooltip column, if present */
  for (i = 0; i < n_columns; i++)
    {
      YadColumn *col = (YadColumn *) g_slist_nth_data (options.list_data.columns, i);

      if (col->type = YAD_COLUMN_TOOLTIP)
	{
	  gtk_tree_view_set_tooltip_column (GTK_TREE_VIEW (list_view), i);
	  break;
	}
    }

  return w;
}

static void
print_col (GtkTreeModel *model, GtkTreeIter *iter, gint num)
{
  YadColumn *col = (YadColumn *) g_slist_nth_data (options.list_data.columns, num);

  switch (col->type)
    {
    case YAD_COLUMN_CHECK:
      {
	gboolean bval;
	gtk_tree_model_get (model, iter, num, &bval, -1);
	g_printf ("%s", bval ? "TRUE" : "FALSE");
	break;
      }
    case YAD_COLUMN_NUM:
      {
	gint64 nval;
	gtk_tree_model_get (model, iter, num, &nval, -1);
	g_printf ("%d", nval);
	break;
      }
    case YAD_COLUMN_TOOLTIP:
    case YAD_COLUMN_TEXT:
      {
	gchar *cval;
	gtk_tree_model_get (model, iter, num, &cval, -1);
	g_printf ("%s", cval);
	break;
      }
    }
  g_printf ("%s", options.common_data.separator);
}

static void
print_selected (GtkTreeModel *model, GtkTreePath *path,
		GtkTreeIter *iter, gpointer data)
{
  gint col = options.list_data.print_column;

  if (col)
    print_col (model, iter, col - 1);
  else
    {
      gint i;
      for (i = 0; i < gtk_tree_model_get_n_columns (model); i++)
	print_col (model, iter, i);
    }
  g_printf ("\n");
}

static void
print_all (GtkTreeModel *model)
{
  GtkTreeIter iter;
  gint cols = gtk_tree_model_get_n_columns (model);

  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      do
	{
	  gint i;
	  for (i = 0; i < cols; i++)
	    print_col (model, &iter, i);
	  g_printf ("\n");
	}
      while (gtk_tree_model_iter_next (model, &iter));
    }
}

void
list_print_result (void)
{
  GtkTreeModel *model;
  gint col = options.list_data.print_column;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (list_view));

  if (options.list_data.print_all)
    {
      print_all (model);
      return;
    }

  if (options.list_data.checkbox)
    {
      // don't check in cycle
      if (col)
	{
	  GtkTreeIter iter;

	  if (gtk_tree_model_get_iter_first (model, &iter))
	    {
	      do
		{
		  gboolean chk;
		  gtk_tree_model_get (model, &iter, 0, &chk, -1);
		  if (chk)
		    {
		      print_col (model, &iter, col - 1);
		      g_printf ("\n");
		    }
		}
	      while (gtk_tree_model_iter_next (model, &iter));
	    }
	}
      else
	{
	  GtkTreeIter iter;

	  if (gtk_tree_model_get_iter_first (model, &iter))
	    {
	      do
		{
		  gboolean chk;
		  gtk_tree_model_get (model, &iter, 0, &chk, -1);
		  if (chk)
		    {
		      gint i;
		      for (i = 0; i < gtk_tree_model_get_n_columns (model); i++)
			print_col (model, &iter, i);
		      g_printf ("\n");
		    }
		}
	      while (gtk_tree_model_iter_next (model, &iter));
	    }
	}
    }
  else
    {
      GtkTreeSelection *sel =
	gtk_tree_view_get_selection (GTK_TREE_VIEW (list_view));

      gtk_tree_selection_selected_foreach (sel, print_selected, NULL);
    }
}