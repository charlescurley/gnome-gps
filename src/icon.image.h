/* A GTK program for showing the latest gps data using libgps. */

/* License: This program is released under the same terms as gpsd
 * itself, i.e under the BSD License. See the file COPYING in the
 * distribution. This program comes with ABSOLUTELY NO WARRANTY. */


/* Our icon in raw format. A screen shot of the program, of course. I
 * then generated the C source with: */

/* gdk-pixbuf-csource gnome-gps.logo.png  > icon.image.c */

/* Then removed the keyword "static" and added a #include for
 * <gtk/gtk.h>. */

extern const guint8 my_pixbuf[];
