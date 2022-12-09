/* A GTK program for showing the latest gps data using libgps. */

/* License: This program is released under the same terms as gpsd
 * itself, i.e under the BSD License. See the file COPYING in the
 * distribution. This program comes with ABSOLUTELY NO WARRANTY. */

char *versionString = "2.0 rc1";

#include <gtk/gtk.h>            /* "apt install libgtk2.0-dev" on
                                 * debian/ubuntu. */

gchar *copyrightString = "Copyright © 2009 to the date of the most recent modification Charles Curley\n";


#define SIZESTATUSSTRINGS 12    /* Also in debug.h */
#define SIZEMODESTRINGS 4       /* Also in debug.h */

/* prototypes */
inline void preserve (GtkEntry *entry, gint index);
char *getStatusString (int status);
void showStatusStrings (void);
inline void sendWatch (void);
void setColor (GdkColor *color);
static gboolean delete_event( GtkWidget *widget,
                              GdkEvent  *event,
                              gpointer   data);
static void destroy( GtkWidget *widget,
                     gpointer   data );

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

static void resynchWrapper ( gpointer   callback_data,
                             guint      callback_action,
                             GtkWidget *menu_item );
static void hostOkCallback (GtkWidget *widget,
                            gpointer   data);
static void hostCancelCallback (GtkWidget *widget,
                                gpointer   data);
static void hostDialog( gpointer   callback_data,
                        guint      callback_action,
                        GtkWidget *menu_item );
static void aboutDialog( gpointer   callback_data,
                         guint      callback_action,
                         GtkWidget *menu_item );
static void setUnits( gpointer   callback_data,
                      guint      callback_action,
                      GtkWidget *menu_item );
static void setGmt( gpointer   callback_data,
                    guint      callback_action,
                    GtkWidget *menu_item );
static void setDegrees( gpointer   callback_data,
                        guint      callback_action,
                        GtkWidget *menu_item );

static void saveState( gpointer   callback_data,
                       guint      callback_action,
                       GtkWidget *menu_item );
static void fontApplyCallback (GtkWidget *widget,
                               gpointer   data);
static void fontOkCallback (GtkWidget *widget,
                            gpointer   data);
static void setFont( gpointer   callback_data,
                     guint      callback_action,
                     GtkWidget *menu_item );
static GtkWidget *get_menubar_menu( GtkWidget  *window );

void showData (void);

static void buildPair (gint index, gchar *labelText, GtkWidget *table,
                       gint left, gint top);
gint gpsPoll (gpointer data);
gchar *saveKeyFile (GKeyFile *keyFile);
void setActiveUnits (void);
void setActiveAngle (void);
int main ( int argc, char *argv[] );
/* end prototypes. */


/* Table layout of the main window. */
#define ROWS 3
#define COLS 4

/* Display factors for the widgets in the table. */
#define XOPTIONS ((GtkAttachOptions) (GTK_FILL|GTK_EXPAND|GTK_SHRINK))
#define YOPTIONS ((GtkAttachOptions) (GTK_FILL|GTK_EXPAND|GTK_SHRINK))
#define XPADDING 5
#define YPADDING 2

#define STRINGBUFFSIZE 1024
