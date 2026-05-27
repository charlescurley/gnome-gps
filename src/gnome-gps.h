/* A GTK program for showing the latest gps data using libgps. */

/* License: This program is released under the same terms as gpsd
 * itself, i.e under the BSD License. See the file COPYING in the
 * distribution. This program comes with ABSOLUTELY NO WARRANTY. */

char *versionString = "2.0";

#include <gtk/gtk.h>            /* "apt install libgtk-4-dev" on
                                 * debian/ubuntu. */

gchar *copyrightString = "Copyright © 2009 to the date of the most recent modification Charles Curley\n";


#define SIZESTATUSSTRINGS 12    /* Also in debug.h */
#define SIZEMODESTRINGS 4       /* Also in debug.h */

/* prototypes */
inline void preserve (GtkEntry *entry, gint index);
char *getStatusString (int status);
void showStatusStrings (void);
inline void sendWatch (void);
void setColor (const char *cssClass);
void cleanup_gps (void);
static gboolean on_close_request (GtkWindow *self, gpointer data);

void formatTrack (double track);
void formatAngle (double theAngle, gchar *buffer);
void formatLat (double latitude);
void formatLong (double longitude);
#if ( GPSD_API_MAJOR_VERSION < 9 )
void formatTime (double time);
#else
void formatTime (timespec_t time);
#endif
void formatAltitude (double altitude);
void formatSpeed (double speed);

/* Menu/action callbacks (GAction style: action, parameter/value, data). */
static void resynchAction (GSimpleAction *action, GVariant *parameter, gpointer data);
static void hostAction    (GSimpleAction *action, GVariant *parameter, gpointer data);
static void aboutAction   (GSimpleAction *action, GVariant *parameter, gpointer data);
static void saveAction    (GSimpleAction *action, GVariant *parameter, gpointer data);
static void quitAction    (GSimpleAction *action, GVariant *parameter, gpointer data);
static void fontAction    (GSimpleAction *action, GVariant *parameter, gpointer data);
static void unitsChangeState    (GSimpleAction *action, GVariant *value, gpointer data);
static void gmtChangeState      (GSimpleAction *action, GVariant *value, gpointer data);
static void magneticChangeState (GSimpleAction *action, GVariant *value, gpointer data);
static void angleChangeState    (GSimpleAction *action, GVariant *value, gpointer data);

/* Host/port dialog button callbacks. */
static void hostOkCallback (GtkWidget *widget, gpointer data);
static void hostCancelCallback (GtkWidget *widget, gpointer data);
static void hostDestroyCallback (GtkWidget *widget, gpointer data);

/* Font handling via CSS + GtkFontDialog. */
static gchar *cssEscapeString (const char *s);
static gchar *pangoDescToCss (PangoFontDescription *desc);
static void applyFontCss (void);
static void onFontChosen (GObject *source, GAsyncResult *res, gpointer data);

/* Menu model and action setup. */
static GMenuModel *buildMenuModel (void);
static void setupActions (GtkApplication *application);

void showData (void);

static void buildPair (gint index, gchar *labelText, GtkWidget *grid,
                       gint left, gint top);
gint gpsPoll (gpointer data);
gchar *saveKeyFile (GKeyFile *keyFile);
static void on_activate (GtkApplication *application, gpointer data);
int main ( int argc, char *argv[] );
/* end prototypes. */


/* Grid layout of the main window. GG for GnomeGps because gps.h uses ROWS. */
#define GGROWS 3
#define GGCOLS 4

/* Display padding for the widgets in the grid. */
#define XPADDING 5
#define YPADDING 2

#define STRINGBUFFSIZE 1024
