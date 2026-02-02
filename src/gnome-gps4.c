/* gnome-gps for gtk4, instead of gtk2. */

/* A GTK program for showing the latest gps data using libgps. */

/* License: This program is released under the same terms as gpsd
 * itself, i.e under the BSD License. See the file COPYING in the
 * distribution. This program comes with ABSOLUTELY NO WARRANTY. */

#include <gtk/gtk.h>

static void
activate (GtkApplication *app,
          gpointer        user_data)
{
  GtkWidget *window;

  window = gtk_application_window_new (app);
  gtk_window_set_title (GTK_WINDOW (window), "Window");
  gtk_window_set_default_size (GTK_WINDOW (window), 200, 200);
  gtk_window_present (GTK_WINDOW (window));
}

int
main (int    argc,
      char **argv)
{
  GtkApplication *app;
  int status;

  app = gtk_application_new ("com.charlescurley.gnome-gps", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
  status = g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref (app);

  return status;
}
