
#include "yad.h"

static GtkWidget *scale;

static void
value_changed_cb (GtkWidget *w, gpointer data)
{
  g_print ("%.0f\n", gtk_range_get_value (GTK_RANGE (w)));
}

GtkWidget *
scale_create_widget (GtkWidget *dlg)
{
  GtkWidget *w;

  if (options.scale_data.min_value >= options.scale_data.max_value)
    {
      g_printerr (_("Maximum value must be greater than minimum value.\n"));
      return NULL;
    }

  if (options.scale_data.value < options.scale_data.min_value ||
      options.scale_data.value > options.scale_data.max_value)
    {
      g_printerr (_("Value out of range.\n"));
      return NULL;
    }

  w = scale = gtk_hscale_new_with_range (options.scale_data.min_value, 
					 options.scale_data.max_value, 
					 options.scale_data.step);
  gtk_range_set_value (GTK_RANGE (w), options.scale_data.value);

  if (options.scale_data.print_partial)
    g_signal_connect (G_OBJECT (w), "value-changed",
		      G_CALLBACK (value_changed_cb), NULL);

  if (options.scale_data.hide_value)
    gtk_scale_set_draw_value (GTK_SCALE (w), FALSE);

  return w;
}

void
scale_print_result (void)
{
  g_print ("%.0f\n", gtk_range_get_value (GTK_RANGE (scale)));
}
