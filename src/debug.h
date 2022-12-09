/* Defines for the debug file. */

#include <gtk/gtk.h>            /* "apt install libgtk2.0-dev" on
                                 * debian/ubuntu. */

#define SIZESTATUSSTRINGS 12    /* Also in gnome-gps.h */
#define SIZEMODESTRINGS 4       /* Also in gnome-gps.h */

/* prototypes */
char *getStatusString (int status);
void showStatusStrings (void);
