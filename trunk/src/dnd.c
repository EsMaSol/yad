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

#include "yad.h"

enum {
  TARGET_UTF8_STRING,
  TARGET_STRING,
  TARGET_TEXT,
  TARGET_COMPOUND_TEXT,
  TARGET_TEXT_PLAIN,
  TARGET_MOZ_URL
};

static GtkTargetEntry tgt[] = {
  {"text/x-moz-url", 0, TARGET_MOZ_URL},
  {"UTF8_STRING", 0, TARGET_UTF8_STRING},
  {"COMPOUND_TEXT", 0, TARGET_COMPOUND_TEXT},
  {"TEXT", 0, TARGET_TEXT},
  {"STRING", 0, TARGET_STRING},
  {"text/plain", 0, TARGET_TEXT_PLAIN}
};

static void
drop_data_cb (GtkWidget *w, GdkDragContext *dc, gint x, gint y,
	      GtkSelectionData *sel, guint info, guint t, gpointer data)
{
  gchar *str = NULL;

  switch (info)
    {
    case TARGET_STRING:
    case TARGET_UTF8_STRING:
    case TARGET_COMPOUND_TEXT:
    case TARGET_TEXT:
      str = gtk_selection_data_get_text (sel);
      break;
    case TARGET_TEXT_PLAIN:
      str = g_strdup (gtk_selection_data_get_data (sel));
      break;
    case TARGET_MOZ_URL:
      {
        GString *str1;
        const guint16 *char_data;
	guint length;
        int i = 0;

        str1 = g_string_new (NULL);
        char_data = (const guint16 *) gtk_selection_data_get_data (sel);
	length = gtk_selection_data_get_length (sel);
        while (i < (length / 2))
          {
            if (char_data[i] == '\n')
              break;
            g_string_append_unichar (str1, (gunichar) char_data[i++]);
          }
        str = g_strdup (str1->str);
        g_string_free (str1, TRUE);
        break;
      }
    }

  if (str)
    {
      if (options.common_data.command)
	{
	  gchar *action = g_strdup_printf ("%s '%s'", options.common_data.command, str);
	  g_spawn_command_line_async (action, NULL);
	  g_free (action);
	}
      else
	{
	  g_printf ("%s\n", str);
	  fflush (stdout);
	}

      g_free (str);
    }

}

void
dnd_init (GtkWidget *w)
{
  gtk_drag_dest_set (w, GTK_DEST_DEFAULT_ALL,
                     tgt, G_N_ELEMENTS (tgt), GDK_ACTION_COPY);
  g_signal_connect (G_OBJECT (w), "drag_data_received",
                    G_CALLBACK (drop_data_cb), NULL);
}
