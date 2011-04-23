/* We'll call it version 1 when gpsd goes to version 3.0. */

char *versionString = "0.91";

/* A GTK program for showing the latest gps data using libgps. */

/* Prerequisites. Any fairly recent version should do.

   aptitude install gcc libgtk2.0-dev

*/

#include <gps.h>                /* Not the same as gpsd.h */

/* Do we recognize the current libgps API? We'll take 5.x or
 * greater. If you change these, change the "comments" string in the
 * aboutDialog as well. */

#if ( GPSD_API_MAJOR_VERSION != 5 )
#error Unknown gps API protocol version; see gps.h for the current value of GPSD_API_MAJOR_VERSION
#endif

#if ( GPSD_API_MAJOR_VERSION >= 5 && GPSD_API_MINOR_VERSION < 0 )
#error Unknown gps API protocol version; see gps.h for the current value of GPSD_API_MINOR_VERSION
#endif

#include <gtk/gtk.h>            /* "aptitude install libgtk2.0-dev" on
                                 * debian/ubuntu. */

#include <math.h>               /* For isnan (), modf (), fabs () */
#include <string.h>             /* For strcpy (), etc. */
#include <unistd.h>             /* getopt stuff */
#include <errno.h>
#include <ctype.h>              /* tolower () */

/* Our icon in raw format. A screen shot of the program, of course. I
 * then generated the C source with: */

/* gdk-pixbuf-csource gnome-gps.logo.png  > icon.image.c */

/* Then removed the keyword "static" and added a #include for
 * <gtk/gtk.h>. */

extern const guint8 my_pixbuf[];

/* prototypes */
inline void preserve (GtkEntry *entry, gint index);
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
void formatTime (double time);
void formatAltitude (double altitude);
void formatSpeed (double speed);

static void resynchWrapper ( gpointer   callback_data,
                             guint      callback_action,
                             GtkWidget *menu_item );
static void resynch (void);
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
static void setUnits( gpointer   callback_data,
                      guint      callback_action,
                      GtkWidget *menu_item );
static void setDegrees( gpointer   callback_data,
                        guint      callback_action,
                        GtkWidget *menu_item );

void showData (void);

static void buildPair (gint index, gchar *labelText, GtkWidget *table,
                       gint left, gint top);
gint gpsPoll (gpointer data);
void saveKeyFile (GKeyFile *keyFile);
void setActiveUnits (void);
void setActiveAngle (void);
int main ( int argc, char *argv[] );
/* end prototypes. */

/* Settings that go into the configuration file. */
typedef struct {
    gchar *port, *host, *angle, *units, *font;
} settings;

/* Table layout of the main window. */
#define ROWS 3
#define COLS 4

/* Display factors for the widgets in the table. */
#define XOPTIONS ((GtkAttachOptions) (GTK_FILL|GTK_EXPAND|GTK_SHRINK))
#define YOPTIONS ((GtkAttachOptions) (GTK_FILL|GTK_EXPAND|GTK_SHRINK))
#define XPADDING 5
#define YPADDING 2

/* Have we detected the loss of the GPS receiver? If so, when we get a
 * new DEVICE message, send a WATCH. */
bool gpsLost = false;

gint tag = 0;
/* GtkWidget is the storage type for widgets */
GtkWidget *window;              /* The main window. */
GtkWidget *vbox;                /* A vertical box, which goes in the
                                 * main window so we can divvy it
                                 * up. */
GtkWidget *menubar;             /* Which goes in the top of the
                                 * vbox. */
GtkWidget *table;               /* Which goes in the middle of the
                                 * vbox. */
GtkProgressBar *progress;       /* Which goes into the bottom of the
                                 * vbox. */
GtkWidget *fontDialog = NULL;   /* The font selector dialog */
GtkItemFactory *item_factory;
GtkAccelGroup *accel_group;

GtkWidget *hostDialogBox = NULL; /* The host/port dialog box. */
GtkWidget *hostEntry = NULL;     /* And the two text entry boxes in it. */
GtkWidget *portEntry = NULL;

GKeyFile *keyFile = NULL;

bool haveConnection = false;
struct gps_data_t gpsdata;

/* Option handling variables. */
extern char *optarg;
extern int optind, opterr, optopt;
gboolean verbose = false;
gboolean haveHome = false;      /* Do we have a home environmental
                                 * variable? */

enum unitsRadio { METRIC = 'm', US = 'u', KNOTS = 'k' } unitsRadio;
char units = US;

enum angleSpec {DEGREES = 'd', MINUTES = 'm', SECONDS = 's' } angleSpec;
char angle = DEGREES;           /* How to display the angles in lat
                                 * and long. */

#define STRINGBUFFSIZE 1024
gchar fixBuff[STRINGBUFFSIZE] = "No fix seen yet";
gchar timeString[STRINGBUFFSIZE] = "No time yet";
gchar latString[STRINGBUFFSIZE] = "No latitude yet";
gchar longString[STRINGBUFFSIZE] = "No longitude yet";
gchar altString[STRINGBUFFSIZE] = "No altitude yet";
gchar speedString[STRINGBUFFSIZE] = "No speed yet";
gchar trackString[STRINGBUFFSIZE] = "No track yet";
gchar keyFileName[STRINGBUFFSIZE];
gchar titleBuff[STRINGBUFFSIZE]; /* the title in the title bar */

/* Defaults, which the user may change later on. */
char hostName[STRINGBUFFSIZE] = "localhost";
char hostPort[STRINGBUFFSIZE] = DEFAULT_GPSD_PORT;

const gchar *baseName;          /* So we can print out only the base
                                   name in the help. */

/* SIZE must be last as we use it to size the array and in related
 * code. */
enum entryNames { LAT, LONG, ALT, SPEED, TRACK,
                  TIME, SIZE } entryNames;

GtkEntry *entries[SIZE];

inline void preserve (GtkEntry *entry, gint index) {
    entries [index] = entry;
}

/* Some colors for various fix states */
GdkColor ThreeDFixColor = {0,      0, 0xffff,      0}; /* green */
GdkColor TwoDFixColor   = {0, 0xff00, 0xff00,      0}; /* yellow */
GdkColor NoFixColor     = {0, 0xff00,      0,      0}; /* red */
GdkColor *oldColor = NULL;

/* sendWatch: tell the gps daemon that we'd like to connect and
 * receive data, thank you. Use this after making a socket
 * (re)connection to the daemon, or after determining that the daemon
 * has lost the gps. You have to resend this after loosing the daemon
 * because otherwise you may never get the notification that you have
 * a new gps. */

inline void sendWatch (void) {
    /* Tell libgps to keep shoveling data at us, and use JSON rather
     * than gpsd's corrected nmea. */
    (void) sleep (1);
    (void) gps_stream(&gpsdata, WATCH_ENABLE | WATCH_JSON, NULL);
}

/* We have to change the background of the window (which gets the
 * labels, since they don't have their own backgrounds), and the
 * individual text entry widgets, which do have their own
 * backgrounds. */
void setColor (GdkColor *color) {
    gint i;

    if ( color != oldColor ) {
        for ( i=0; i < SIZE; i++) {
            if (entries[i] != NULL) {
                gtk_widget_modify_bg (GTK_WIDGET (entries[i]), 0, color);
            }
        }

        gtk_widget_modify_bg (GTK_WIDGET (window),   0, color);
        gtk_widget_modify_bg (GTK_WIDGET (menubar),  0, color);
        gtk_widget_modify_bg (GTK_WIDGET (progress), 0, color);

        oldColor = color;
    }
}

static gboolean delete_event( GtkWidget *widget,
                              GdkEvent  *event,
                              gpointer   data) {
    /* If you return FALSE in the "delete_event" signal handler, GTK
     * will emit the "destroy" signal. Returning TRUE means you don't
     * want the window to be destroyed. This is useful for popping up
     * 'are you sure you want to quit?' type dialogs. */

    return FALSE;
}

static void destroy( GtkWidget *widget,
                     gpointer   data ) {
    if (haveConnection == true) {
        (void) gps_stream(&gpsdata, WATCH_DISABLE, NULL);
        (void) gps_close (&gpsdata);
        haveConnection = false;
    }

    if (tag != 0) {
        g_source_remove (tag);
    }

    gtk_main_quit ();
}

/* See http://en.wikipedia.org/wiki/Boxing_the_compass. This isn't the
 * same algorithm. We use scaled integer math, so it's faster and more
 * suitable for simpler devices. */
void formatTrack (double track) {
    char *dirString;
    int dir = (int) track * 10;
    dir += 112;             /* handle inadvertent rounding due to
                             * float to int conversion. */
    dir /= 225;

    switch (dir) {
    case 0:
        dirString = "N";
        break;

    case 1:
        dirString = "NNE";
        break;

    case 2:
        dirString = "NE";
        break;

    case 3:
        dirString = "ENE";
        break;

    case 4:
        dirString = "E";
        break;

    case 5:
        dirString = "ESE";
        break;

    case 6:
        dirString = "SE";
        break;

    case 7:
        dirString = "SSE";
        break;

    case 8:
        dirString = "S";
        break;

    case 9:
        dirString = "SSW";
        break;

    case 10:
        dirString = "SW";
        break;

    case 11:
        dirString = "WSW";
        break;

    case 12:
        dirString = "W";
        break;

    case 13:
        dirString = "WNW";
        break;

    case 14:
        dirString = "NW";
        break;

    case 15:
        dirString = "NNW";
        break;

    case 16:
        dirString = "N";
        break;

    default:
        dirString = "oops!";

    }

    (void) snprintf (trackString, STRINGBUFFSIZE,
                     "%03.0f° true %s", track, dirString);
    gtk_entry_set_text(entries[TRACK], trackString );
}

/* Factored from both formatLat and formatLong */
void formatAngle (double theAngle, gchar *buffer) {
    double minutes;
    double seconds;

    switch (angle) {

        /* Use the default to silently handle errors and fall through
         * to the substitute case. */
    default:
        angle = DEGREES;
    case DEGREES:
        (void) snprintf (buffer, STRINGBUFFSIZE, "%.6f° ", theAngle);
        break;

    case MINUTES:
        minutes = modf (theAngle, &theAngle);
        (void) snprintf (buffer, STRINGBUFFSIZE,
                         "%.f° %.4f' ", theAngle, minutes*60);
        break;

    case SECONDS:
        minutes = modf (theAngle, &theAngle);
        minutes *= 60;
        seconds = modf (minutes, &minutes);
        (void) snprintf (buffer, STRINGBUFFSIZE,
                         "%.f° %.f' %.2f\" ", theAngle, minutes, seconds*60);
        break;
    }
}

void formatLat (double latitude) {
    double latAbs = fabs (latitude);
    formatAngle (latAbs, latString);
    (void) strncat (latString, latitude < 0 ? "S" : latitude > 0 ? "N" : "" , 1);
    gtk_entry_set_text(entries[LAT], latString );
}

void formatLong (double longitude) {
    double longAbs = fabs (longitude);
    formatAngle (longAbs, longString);
    (void) strncat (longString, longitude < 0 ? "W" : longitude > 0 ? "E" : "" , 1);
    gtk_entry_set_text(entries[LONG], longString );
}

void formatTime (double time) {
    if (isnan(time)==0) {
        (void) unix_to_iso8601(time, timeString,
                               (int) sizeof(timeString));
    } else {
        (void) strcpy(timeString,"n/a");
    }
    gtk_entry_set_text(entries[TIME], timeString );
}

void formatAltitude (double altitude) {
    if (units != METRIC) {
        altitude *= METERS_TO_FEET;
    }

    (void) snprintf (altString, STRINGBUFFSIZE,
                     units != METRIC ? "%.0f feet" : "%.1f meters",
                     altitude);
    gtk_entry_set_text(entries[ALT], altString );
}

/* Speed in meters per second. */
void formatSpeed (double speed) {
    gchar *formatString = NULL;

    switch (units) {
    case KNOTS:
        speed *= MPS_TO_KNOTS;
        formatString = speed < 10.0 ? "%.1f knots"
            : "%f knots" ;
        break;

    case US:
        speed *= MPS_TO_MPH;
        formatString = speed < 10.0 ? "%.1f miles per hour"
            : "%.0f miles per hour" ;
        break;

        /* Use the default to silently handle errors and fall through
         * to the substitute case. */
    default:
        units = METRIC;

    case METRIC:
        speed *= MPS_TO_KPH;
        formatString =  speed < 10.0 ? "%.1f km per hour"
            : "%.0f km per hour";
        break;
    }

    (void) snprintf (speedString, STRINGBUFFSIZE, formatString, speed);
    gtk_entry_set_text(entries[SPEED], speedString );
}

/* This exists so we can call resynch () from the menu. We don't
 * actually use the parameters at all. We call resynch () elsewhere in
 * the program. */
static void resynchWrapper ( gpointer   callback_data,
                             guint      callback_action,
                             GtkWidget *menu_item ) {
    resynch ();
}

static void resynch (void) {
    int ret = 0;

    setColor (&NoFixColor);

    if (haveConnection == true) {
        (void) gps_stream(&gpsdata, WATCH_DISABLE, NULL);
        (void) gps_close (&gpsdata);
        haveConnection = false;
    }

    gtk_progress_bar_set_fraction (progress, 0.0);
    ret = gps_open(hostName, hostPort, &gpsdata);
    if (ret != 0) {
        if (verbose) {
            if (errno < 1) {
                (void) fprintf (stderr, "%s: gpsd error: %s %s:%s.\n",
                                baseName, gps_errstr(errno),
                                hostName, hostPort);
            } else {
                perror (baseName);
            }
        }
        gtk_progress_bar_set_text (progress, "No gpsd connection!");
        haveConnection = false;
    } else {
        /* If we got here, we're good to go. */
        gtk_progress_bar_set_text (progress, "Ahhh, a gpsd connection!");
        gpsLost = false;
        haveConnection = true;
        sendWatch ();
    }
}

static void hostOkCallback (GtkWidget *widget,
                            gpointer   data) {
    const gchar *sandbox = NULL; /* look but don't touch */

    /* Get the contents of the two entry widgets */
    sandbox = gtk_entry_get_text (GTK_ENTRY (hostEntry));

    if ((sandbox != NULL) && (strlen (sandbox) < STRINGBUFFSIZE) && (strlen (sandbox))) {
        (void) strncpy (hostName, sandbox, STRINGBUFFSIZE);
    }

    sandbox = gtk_entry_get_text (GTK_ENTRY (portEntry));
    if ((sandbox != NULL) && (strlen (sandbox) < STRINGBUFFSIZE) && (strlen (sandbox))) {
        (void) strncpy (hostPort, sandbox, STRINGBUFFSIZE);
    }

    gtk_widget_destroy (hostDialogBox);
}

static void hostCancelCallback (GtkWidget *widget,
                                gpointer   data) {
    gtk_widget_destroy (hostDialogBox);
}

static void hostDialog( gpointer   callback_data,
                        guint      callback_action,
                        GtkWidget *menu_item ) {
    GtkWidget *table = NULL;
    GtkWidget *child = NULL;
    GtkWidget *okButton = NULL;
    GtkWidget *cancelButton = NULL;

    hostDialogBox = gtk_dialog_new ();
    table = gtk_table_new( 2, 2, FALSE ); /* 2 rows, 2 cols */

    /* Put the table in the vbox */
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (hostDialogBox)->vbox),
                        table, TRUE, TRUE, 0);

    {
        /* Creates a new right aligned label. */
        child = gtk_label_new( "Host" );
        gtk_misc_set_alignment(GTK_MISC(child), 1.0f, 0.5f);

        /* Instead of gtk_container_add, we pack this button into the
         * table, which has been packed into the window. */
        gtk_table_attach(GTK_TABLE (table), child, 0, 1, 0, 1,
                         XOPTIONS, YOPTIONS, XPADDING, YPADDING );

        /* And now the non-editable text box to show it. */
        hostEntry = gtk_entry_new ();
        gtk_editable_set_editable(GTK_EDITABLE (hostEntry), true);
        gtk_entry_set_text(GTK_ENTRY (hostEntry), hostName );
        gtk_signal_connect (GTK_OBJECT(hostEntry), "activate",
                            GTK_SIGNAL_FUNC(hostOkCallback), NULL);

        /* Instead of gtk_container_add, we pack this text entry into the
         * table, which has been packed into the window. */
        gtk_table_attach(GTK_TABLE (table), hostEntry, 1, 2, 0, 1,
                         XOPTIONS, YOPTIONS, XPADDING, YPADDING );
    }

    {
        /* Creates a new right aligned label. */
        child = gtk_label_new( "Port" );
        gtk_misc_set_alignment(GTK_MISC(child), 1.0f, 0.5f);

        /* Instead of gtk_container_add, we pack this button into the
         * table, which has been packed into the window. */
        gtk_table_attach(GTK_TABLE (table), child, 0, 1, 1, 2,
                         XOPTIONS, YOPTIONS, XPADDING, YPADDING );

        /* And now the non-editable text box to show it. */
        portEntry = gtk_entry_new ();
        gtk_editable_set_editable(GTK_EDITABLE (portEntry), true);
        gtk_entry_set_text(GTK_ENTRY (portEntry), hostPort );
        gtk_signal_connect (GTK_OBJECT(portEntry), "activate",
                            GTK_SIGNAL_FUNC(hostOkCallback), NULL);

        /* Instead of gtk_container_add, we pack this text entry into the
         * table, which has been packed into the window. */
        gtk_table_attach(GTK_TABLE (table), portEntry, 1, 2, 1, 2,
                         XOPTIONS, YOPTIONS, XPADDING, YPADDING );
    }
    {
        okButton = gtk_button_new_from_stock (GTK_STOCK_OK);
        /* Connect the "clicked" signal of the button to our callback */
        g_signal_connect (G_OBJECT (okButton), "clicked",
                          G_CALLBACK (hostOkCallback), (gpointer) "OK");
        gtk_box_pack_start (GTK_BOX (GTK_DIALOG (hostDialogBox)->action_area),
                            okButton, TRUE, TRUE, 0);
    }
    {
        cancelButton = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
        /* Connect the "clicked" signal of the button to our callback */
        g_signal_connect (G_OBJECT (cancelButton), "clicked",
                          G_CALLBACK (hostCancelCallback), (gpointer) "Cancel");
        gtk_box_pack_start (GTK_BOX (GTK_DIALOG (hostDialogBox)->action_area),
                            cancelButton, TRUE, TRUE, 0);
    }

    /* Always remember this step, this tells GTK that our preparation
     * for this window is complete, and it can now be displayed. */
    gtk_widget_show_all (hostDialogBox);
}

/* Some properties string arrays for the About dialog. */
gchar *authors[2] =     { "Charles Curley, http://www.charlescurley.com/", NULL };
gchar *documentors[2] = { "Charles Curley, http://www.charlescurley.com/", NULL };

static void aboutDialog( gpointer   callback_data,
                         guint      callback_action,
                         GtkWidget *menu_item ) {
    gtk_show_about_dialog (GTK_WINDOW (window),
                           "authors", authors,
                           "comments", "A simple GTK+ GPS monitor.\n\nRed: No fix.\n"
                           "Yellow: Two dimensional fix.\nGreen: Three dimensional fix.\n"
                           "Libgps API version 5.0.\n"
                           "\"Save\" to save settings such as font and units.",
                           "copyright", "Copyright © 2009-2011 Charles Curley.",
                           "documenters", documentors,
                           "license", "This program is released under the same terms as gpsd "
                           "itself, i.e under the BSD License. See the file COPYING in the "
                           "distribution.\nThis program comes with ABSOLUTELY NO WARRANTY.\n",
                           "program-name", baseName,
                           "translator-credits", "Your name here?",
                           "website", "http://www.charlescurley.com/",
                           "wrap-license", TRUE,
                           NULL);
}

/* Build the menu */
static GtkItemFactoryEntry menu_items[] = {
    /* menu hierarchy             Accel    Function    action    Type  */
    { "/_File",                      NULL,        NULL,       0, "<Branch>" },
    { "/File/_Font",            "<ctrl>f",     setFont,       0, "<StockItem>", GTK_STOCK_SELECT_FONT },
    { "/File/_Save",            "<ctrl>s",   saveState,       0, "<StockItem>", GTK_STOCK_SAVE },
    { "/File/_Quit",            "<ctrl>q",     destroy,       0, "<StockItem>", GTK_STOCK_QUIT },
    { "/_Units",                     NULL,        NULL,       0, "<Branch>" },
    { "/Units/_Metric",         "<ctrl>m",    setUnits,  METRIC, "<RadioItem>"          },
    { "/Units/_US",             "<ctrl>i",    setUnits,      US, "/Units/Metric"        },
    { "/Units/_Knots",          "<ctrl>k",    setUnits,   KNOTS, "/Units/Metric"        },
    { "/_Degrees",                   NULL,        NULL,       0, "<Branch>" },
    { "/Degrees/_ddd.dddddd",   "<ctrl>d",  setDegrees, DEGREES, "<RadioItem>"          },
    { "/Degrees/ddd _mm.mmmm",  "<ctrl>n",  setDegrees, MINUTES, "/Degrees/ddd.dddddd"  },
    { "/Degrees/ddd mm _ss.ss", "<ctrl>s",  setDegrees, SECONDS, "/Degrees/ddd.dddddd"  },
    { "/_Host",                      NULL,        NULL,       0, "<Branch>" },
    { "/_Host/_Resynch",        "<ctrl>r",  resynchWrapper,   0, "<StockItem>", GTK_STOCK_REFRESH },
    { "/_Host/_Host",           "<ctrl>h",  hostDialog,       0, "<StockItem>", GTK_STOCK_NETWORK },
    { "/_Help",                      NULL,        NULL,       0, "<Branch>" },
    { "/_Help/_About",          "<ctrl>a", aboutDialog,       0, "<StockItem>", GTK_STOCK_ABOUT },
};

/* Now some functions for the menu */
static void setUnits( gpointer   callback_data,
                      guint      callback_action,
                      GtkWidget *menu_item ) {
    units = callback_action;
}

/* Now some functions for the menu */
static void setDegrees( gpointer   callback_data,
                        guint      callback_action,
                        GtkWidget *menu_item ) {
    angle = callback_action;
}

/* Now some functions for the menu */
static void saveState( gpointer   callback_data,
                       guint      callback_action,
                       GtkWidget *menu_item ) {
    if (haveHome) {
        saveKeyFile (keyFile);
    }
}

static void fontApplyCallback (GtkWidget *widget,
                               gpointer   data) {
    gchar *theFont = NULL;

    theFont = gtk_font_selection_dialog_get_font_name
        (GTK_FONT_SELECTION_DIALOG (fontDialog));

    if (theFont != NULL) {
        GtkRcStyle *style;
        GdkColor *color;
        gint i;

        style = gtk_rc_style_new ();
        pango_font_description_free (style->font_desc);
        style->font_desc = pango_font_description_from_string (theFont);

        for ( i=0; i < SIZE; i++) {
            if (entries[i] != NULL) {
                gtk_widget_modify_style (GTK_WIDGET (entries[i]), style);
            }
        }
        gtk_widget_modify_style (GTK_WIDGET (progress), style);

        g_free (theFont);

        /* Now fox the color setting code. It actually changes colors
         * only if the parameter color != the old color we saved last
         * time it ran. */
        color = oldColor;
        oldColor = NULL;
        setColor (color);
    }
}

static void fontOkCallback (GtkWidget *widget,
                            gpointer   data) {
    fontApplyCallback (widget, data);
    gtk_widget_destroy (fontDialog);
}

static void setFont( gpointer   callback_data,
                     guint      callback_action,
                     GtkWidget *menu_item ) {
    gchar title[STRINGBUFFSIZE];
    const gchar *fontName;
    GtkStyle *style;

    (void) snprintf (title, STRINGBUFFSIZE, "%s: Select a font", baseName);
    fontDialog = gtk_font_selection_dialog_new (title);

    /* Now we jump through some hoops to get the existing font so we
     * can set the default to it instead of the system default. We use
     * one of the text entry widgets because their fonts are set by
     * this dialog. */
    style = gtk_widget_get_style (GTK_WIDGET (entries[TIME]));
    fontName = pango_font_description_to_string (style->font_desc);

    (void) gtk_font_selection_dialog_set_font_name
        (GTK_FONT_SELECTION_DIALOG (fontDialog), fontName);
    gtk_font_selection_dialog_set_preview_text (
        GTK_FONT_SELECTION_DIALOG(fontDialog), timeString);

    g_signal_connect (GTK_FONT_SELECTION_DIALOG (fontDialog)->ok_button,
                      "clicked", G_CALLBACK (fontOkCallback),
                      NULL);
    g_signal_connect (GTK_FONT_SELECTION_DIALOG (fontDialog)->apply_button,
                      "clicked", G_CALLBACK (fontApplyCallback),
                      NULL);
    g_signal_connect_swapped (GTK_FONT_SELECTION_DIALOG (fontDialog)->cancel_button,
                              "clicked", G_CALLBACK (gtk_widget_destroy),
                              fontDialog);

    gtk_widget_show_all (fontDialog);
}

static gint nmenu_items = sizeof (menu_items) / sizeof (menu_items[0]);

/* Returns a menubar widget from the menu */
static GtkWidget *get_menubar_menu( GtkWidget  *window ) {
    /* Make an accelerator group (shortcut keys) */
    accel_group = gtk_accel_group_new ();

    /* Make an ItemFactory (that makes a menubar) */
    item_factory = gtk_item_factory_new (GTK_TYPE_MENU_BAR, "<main>",
                                         accel_group);

    /* This function generates the menu items. Pass the item factory,
       the number of items in the array, the array itself, and any
       callback data for the the menu items. */
    gtk_item_factory_create_items (item_factory, nmenu_items, menu_items, NULL);

    /* Attach the new accelerator group to the window. */
    gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);

    /* Finally, return the actual menu bar created by the item factory. */
    return gtk_item_factory_get_widget (item_factory, "<main>");
}

/* Our display function. Nested case statements. */
void showData (void) {
    char tmpBuff[STRINGBUFFSIZE];   /* generic temporary holding. */
    char *fixStatus = "DGPS ";  /* Default, if we even use it. */

    /* Some things we ignore. */
    if (gpsdata.set & POLICY_SET) {
        gpsdata.set &= ~(POLICY_SET);
        return;
    }

    /* Fill in receiver type. Detect missing gps receiver. */
    if (gpsdata.set & (DEVICE_SET)) {
        if (gpsdata.dev.activated < 1.0) {
            sendWatch ();
            gpsLost = true;

            gtk_progress_bar_set_fraction (progress, 0.0);
            gtk_progress_bar_set_text (progress, "GPS lost!");
            setColor (&NoFixColor);

            if (verbose) {
                (void) fprintf (stderr, "gps lost.\n");
                (void) strcpy (tmpBuff, "driver = nil: subtype = nil: activated = 0.0");
            }
            (void) snprintf (titleBuff, STRINGBUFFSIZE,
                             "%s: a simple GTK+ GPS monitor", baseName);
            gtk_window_set_title (GTK_WINDOW (window), titleBuff);

        } else {
            gpsLost = false;

            /* Old versions of printf would break if presented with a string
             * of 0 length. OK, I'm paranoid, deal with it. */
            if (strlen (gpsdata.dev.driver)) {
                (void) snprintf(tmpBuff, sizeof(tmpBuff), "%s GPS Found!", gpsdata.dev.driver);
                (void) snprintf (titleBuff, STRINGBUFFSIZE,
                                 "%s: %s; %s",
                                 baseName, gpsdata.dev.driver, gpsdata.dev.subtype);
                gtk_window_set_title (GTK_WINDOW (window), titleBuff);
            } else {
                (void) strcpy (tmpBuff, "GPS Found!");
            }
            sendWatch ();
            gtk_progress_bar_set_text (progress, tmpBuff);
            if (verbose) {
                (void) snprintf(tmpBuff, sizeof(tmpBuff),
                                "driver = %s: subtype = %s: activated = %f",
                                gpsdata.dev.driver, gpsdata.dev.subtype,
                                gpsdata.dev.activated);
                (void) printf ("gps found.\n");
            }
        }

        if (verbose) {
            (void) printf ("set 0x%08x, (device) GPS Driver: %s\n",
                           (unsigned int) gpsdata.set, tmpBuff);
        }

        return;
    }

    if ((gpsdata.set & DEVICELIST_SET) && verbose) {
        if (gpsdata.devices.ndevices == 1) {
            if (gpsdata.devices.list[0].activated < 1.0) {
                (void) strcpy (tmpBuff, "driver = nil: subtype = nil: activated = 0.0");
            } else {
                (void) snprintf(tmpBuff, sizeof(tmpBuff), "driver = %s: subtype = %s: activated = %f",
                                gpsdata.devices.list[0].driver,
                                gpsdata.devices.list[0].subtype,
                                gpsdata.devices.list[0].activated);
            }
        } else {
            /* We have multiple devices, which I've never seen, so
             * punt. */
            (void) snprintf(tmpBuff, sizeof(tmpBuff), "%d devices",
                            gpsdata.devices.ndevices);
        }
        (void) printf ("set 0x%08x, (Devices) GPS Driver: %s\n",
                       (unsigned int) gpsdata.set, tmpBuff);

        return;
    }

    if (gpsdata.set & TIME_SET) {
        formatTime (gpsdata.fix.time);
    }

    if (gpsdata.set & VERSION_SET && (verbose != false)) {
        (void) printf ("set 0x%08x, GPSD version: %s Rev: %s, Protocol %d.%d\n",
                       (unsigned int) gpsdata.set,
                       gpsdata.version.release,
                       gpsdata.version.rev,
                       gpsdata.version.proto_major,
                       gpsdata.version.proto_minor);
    }

    if (gpsdata.set & SPEED_SET) {
        formatSpeed (gpsdata.fix.speed);
    }

    if (gpsdata.set & TRACK_SET) {
        formatTrack (gpsdata.fix.track);
    }

    /* A nice and unusual use of a progress bar, if I say so
     * myself. */
    if (gpsdata.set & SATELLITE_SET) {
        gchar banner[STRINGBUFFSIZE];

        /* Guard against division of or by 0. */
        if (gpsdata.satellites_visible > 0 && gpsdata.satellites_used > 0) {
            gtk_progress_bar_set_fraction (progress,
                                           (gdouble) gpsdata.satellites_used/
                                           (gdouble) gpsdata.satellites_visible);

            (void) snprintf (banner, STRINGBUFFSIZE,
                             "%2d satellites visible, %2d used in the fix.",
                             gpsdata.satellites_visible,
                             gpsdata.satellites_used);
        } else {
            gtk_progress_bar_set_fraction (progress, 0.0);
            if (gpsdata.satellites_visible > 0) {
                (void) snprintf (banner, STRINGBUFFSIZE,
                                 "%2d satellites visible, no fix.",
                                 gpsdata.satellites_visible);
            } else {
                (void) snprintf (banner, STRINGBUFFSIZE,
                                 "No satellites visible, no fix.");
            }
        }
        gtk_progress_bar_set_text (progress, banner);

        if (verbose) {
            (void) printf ("set 0x%08x, %s\n", (unsigned int) gpsdata.set,
                           banner);
        }
        /* Something is fishy. If we've had satellites
         * visible, and then lose them,
         * gpsdata.satellites_visible still reports 1, but
         * not 0. I think. This is to re-set it just in
         * case. */
        /* gpsdata.satellites_visible = 0; */
    }

    if (gpsdata.set & STATUS_SET) {
        switch (gpsdata.status) {
        case STATUS_NO_FIX:
            (void) strcpy (fixBuff, "No fix");
            setColor (&NoFixColor);
            break;

        case STATUS_FIX:
            (void) strcpy (fixBuff, "No fix");
            fixStatus = "";

        case STATUS_DGPS_FIX:

            if (gpsdata.set & MODE_SET) {
                switch (gpsdata.fix.mode) {
                case MODE_NOT_SEEN:
                    (void) strcpy (fixBuff, "Fix not yet seen");
                    setColor (&NoFixColor);
                    break;

                case MODE_NO_FIX:
                    (void) strcpy (fixBuff, "No fix yet");
                    setColor (&NoFixColor);
                    break;

                case MODE_2D:
                    if ((gpsdata.set & LATLON_SET)) {
                        formatLat (gpsdata.fix.latitude);
                        formatLong (gpsdata.fix.longitude);
                        setColor (&TwoDFixColor);

                        if (verbose) {
                            (void) snprintf (fixBuff, STRINGBUFFSIZE,
                                             "2D %sFix, la %f, lo %f",
                                             fixStatus,
                                             gpsdata.fix.latitude,
                                             gpsdata.fix.longitude);
                        }
                    }
                    break;

                case MODE_3D:
                    if ((gpsdata.set & (LATLON_SET|ALTITUDE_SET))) {
                        formatLat (gpsdata.fix.latitude);
                        formatLong (gpsdata.fix.longitude);

                        /* Only if we have a fix are the details of it useful. Altitude is
                           only meaningful on a 3D fix. */
                        formatAltitude (gpsdata.fix.altitude);
                        setColor (&ThreeDFixColor);
                        if (verbose) {
                            (void) snprintf (fixBuff, STRINGBUFFSIZE,
                                             "3D %sFix, la %f, lo %f, %f",
                                             fixStatus,
                                             gpsdata.fix.latitude,
                                             gpsdata.fix.longitude,
                                             gpsdata.fix.altitude);
                        }

                    }
                    break;

                default:
                    (void) fprintf (stderr, "%s: Catastrophic error: Invalid mode %d.\n",
                                    baseName, gpsdata.fix.mode);
                    setColor (&NoFixColor);
                    return;
                } /* fix.mode */
            } /* MODE_SET */
            break;

        default:
            (void) fprintf (stderr, "Catastrophic error: Invalid status %d.\n",
                            gpsdata.status);
            setColor (&NoFixColor);
            return;
        }

        if (verbose) {
            (void) printf ("set 0x%08x, mode %d, %s, %s.\n",
                           (unsigned int) gpsdata.set,
                           gpsdata.fix.mode,
                           timeString,
                           fixBuff);
        }
    } /* STATUS_SET */
}

/* Build a pair of display widgets, a label followed by an entry
 * box. */
static void buildPair (gint index, gchar *labelText, GtkWidget *table,
                       gint left, gint top) {
    GtkWidget *child;
    left <<= 1;                 /* Convert pair co-ordinates to row
                                 * co-ordinates by multiplying by
                                 * 2. */

    /* Creates a new right aligned label. */
    child = gtk_label_new( labelText );
    gtk_misc_set_alignment(GTK_MISC(child), 1.0f, 0.5f);

    /* Instead of gtk_container_add, we pack this button into the
     * table, which has been packed into the window. */
    gtk_table_attach(GTK_TABLE (table), child, left, left+1, top, top+1,
                     XOPTIONS, YOPTIONS, XPADDING, YPADDING );

    /* And now the non-editable text box to show it. */
    child = gtk_entry_new ();
    gtk_editable_set_editable(GTK_EDITABLE (child), true);
    gtk_entry_set_text(GTK_ENTRY (child), "Not Yet Seen" );

    /* Now ask for a slightly longer time widget. */
    gtk_entry_set_width_chars (GTK_ENTRY (child), left ? 22 : 17);

    /* Instead of gtk_container_add, we pack this text entry into the
     * table, which has been packed into the window. */
    gtk_table_attach(GTK_TABLE (table), child, left+1, left+2, top, top+1,
                     XOPTIONS, YOPTIONS, XPADDING, YPADDING );

    /* Add it to the table so we can access it later to change its
     * text and color. */
    preserve (GTK_ENTRY (child), index);
}

/* Return true so we get called forever. */
gint gpsPoll (gpointer data) {
    if (haveConnection == false ) {
        resynch ();
        return (true);
    }

    if (gps_waiting (&gpsdata, 200000) == true) {
        errno = 0;
        if (gps_read (&gpsdata) == -1) {
            if (errno == 0) {
                gtk_progress_bar_set_text (progress, "Lost contact with gpsd.");
            } else {
                gtk_progress_bar_set_text (progress, "Something went wrong.");
            }
            gtk_progress_bar_set_fraction (progress, 0.0);
            resynch ();
        } else {
            showData ();
        }
    }

    return (true);
}

/* Collect our changed settings and preserve them. */
void saveKeyFile (GKeyFile *keyFile) {
    FILE *file;
    gchar *string = NULL;
    gsize length = 0;
    gsize size = 0;
    gchar sandbox[128];
    GtkStyle *style;
    const gchar *fontName;

    if (haveHome == false) {
        return;
    }

    g_key_file_set_value (keyFile, baseName, "host", hostName);
    g_key_file_set_value (keyFile, baseName, "port", hostPort);

    sandbox [1] = '\0';
    sandbox [0] = angle;
    g_key_file_set_value (keyFile, baseName, "angle", sandbox);

    sandbox [0] = units;
    g_key_file_set_value (keyFile, baseName, "units", sandbox);

    /* Now we jump through some hoops to get the existing font so we
     * save it. We use one of the text entry widgets because their
     * fonts are set by this dialog. */
    style = gtk_widget_get_style (GTK_WIDGET (entries[TIME]));
    fontName = pango_font_description_to_string (style->font_desc);
    g_key_file_set_value (keyFile, baseName, "font", fontName);

    string = g_key_file_to_data (keyFile, &length, NULL);

    if (length > 0 && string != NULL) {
        if (verbose) {
            (void) printf ("Length: %d. Configuration string: '%s'\n",
                           (int) length, string);
        }

        file = fopen (keyFileName, "w");
        if (file == NULL) {
            (void) fprintf (stderr, "Could not open file '%s'.\n",
                            keyFileName);
            return;
        }
        size = fwrite (string, length, 1, file);
        if (size != 1) {
            (void) fprintf (stderr, "Failure writing to file '%s'.\n",
                            keyFileName);
            return;
        }
        (void) fclose (file);
    }

}

/* Having initialized the angles, we had better set the menu entry to
 * selected as well. See the notes where 'units' is defined. */
void setActiveAngle (void) {
    switch (angle) {

        /* Use the default to silently handle errors and fall through
         * to the substitute case. */
    default:
        angle = DEGREES;

    case DEGREES:
        gtk_check_menu_item_set_active(
            GTK_CHECK_MENU_ITEM (gtk_item_factory_get_item (item_factory, "/Degrees/ddd.dddddd")),
            TRUE);
        break;

    case MINUTES:
        gtk_check_menu_item_set_active(
            GTK_CHECK_MENU_ITEM (gtk_item_factory_get_item (item_factory, "/Degrees/ddd mm.mmmm")),
            TRUE);
        break;

    case SECONDS:
        gtk_check_menu_item_set_active(
            GTK_CHECK_MENU_ITEM (gtk_item_factory_get_item (item_factory, "/Degrees/ddd mm ss.ss")),
            TRUE);
        break;

    }
}

/* Having initialized the units, we had better set the menu entry to
 * selected as well. See the notes where 'units' is defined. */
void setActiveUnits (void) {
    switch (units) {

        /* Use the default to silently handle errors and fall through
         * to the substitute case. */
    default:
        units = US;
    case US:
        gtk_check_menu_item_set_active(
            GTK_CHECK_MENU_ITEM (gtk_item_factory_get_item (item_factory, "/Units/US")),
            TRUE);
        break;

    case KNOTS:
        gtk_check_menu_item_set_active(
            GTK_CHECK_MENU_ITEM (gtk_item_factory_get_item (item_factory, "/Units/Knots")),
            TRUE);
        break;

    case METRIC:
        gtk_check_menu_item_set_active(
            GTK_CHECK_MENU_ITEM (gtk_item_factory_get_item (item_factory, "/Units/Metric")),
            TRUE);
        break;
    }
}

int main ( int   argc,
           char *argv[] ) {
    int ret = 0;
    int i;

    /* gfile variables. Swiped from
     * http://www.gtkbook.com/tutorial.php?page=keyfile. I may have to
     * buy the book. */
    GKeyFileFlags flags;
    GError *error = NULL;
    settings *conf = NULL;

    /* Clean out the list of entries so we can check for ones not
     * initialized. */
    for ( i=0 ; i < SIZE; i++) {
        entries[i] = NULL;
    }

    baseName = g_get_application_name ();
    if (baseName == NULL) {
        baseName = g_path_get_basename (g_strdup (argv[0]));
    }

    (void) printf ("\n%s: Copyright © 2009-2011 Charles Curley\n", baseName);

    (void) printf ("This program is released under the same terms as gpsd itself, i.e.\n");
    (void) printf ("under the BSD License. See the file Copying in the gpsd distribution.\n");

    (void) printf ("\nThis program comes with ABSOLUTELY NO WARRANTY.\n");

    /* gfile setup. Do this first, then see if the user overrides with
     * command line options later. */
    {
        const gchar *home = NULL;

        home = g_getenv ("HOME");
        if (home == NULL) {
            (void) fprintf (stderr, "%s: HOME environment variable not set.\n",
                            baseName);
            (void) fprintf (stderr, "%s needs it to find its configuration file.\n",
                            baseName);
        } else {
            if (strlen (home) + strlen (baseName) + 2 >= STRINGBUFFSIZE) {
                (void) fprintf (stderr, "%s: resource file name is too long.\n",
                                baseName);
                return (3);
            }
            haveHome = true;

            (void) strncpy (keyFileName, home, STRINGBUFFSIZE);
            (void) strncat (keyFileName, "/.", STRINGBUFFSIZE - strlen (keyFileName));
            (void) strncat (keyFileName, baseName,
                            STRINGBUFFSIZE - strlen (keyFileName));

            keyFile = g_key_file_new ();
            flags = G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS;

            /* Load the GKeyFile from keyfile.conf if it exists. If it
             * doesn't exist, punt. */
            (void) g_key_file_load_from_file (keyFile, keyFileName, flags, &error);

            /* Create a new Settings object. If you are using GTK+ 2.8 or below, you should
             * use g_new() or g_malloc() instead! */
            conf = g_slice_new (settings);

            conf->host = g_key_file_get_string ( keyFile, baseName,
                                                 "host", NULL);
            if (conf->host != NULL && strlen (conf->host) > 0) {
                (void) strncpy (hostName, conf->host, STRINGBUFFSIZE);
            }

            conf->port = g_key_file_get_string ( keyFile, baseName,
                                                 "port", NULL);
            if (conf->port != NULL && strlen (conf->port) > 0) {
                (void) strncpy (hostPort, conf->port, STRINGBUFFSIZE);
            }

            conf->angle = g_key_file_get_string ( keyFile, baseName,
                                                  "angle", NULL);
            if (conf->angle != NULL && strlen (conf->angle) > 0) {
                /* Assume some bozo will edit it. */
                angle = tolower (conf->angle[0]);
            }

            conf->units = g_key_file_get_string ( keyFile, baseName,
                                                  "units", NULL);
            if (conf->units != NULL && strlen (conf->units) > 0) {
                /* Assume some bozo will edit it. */
                units = tolower (conf->units[0]);
            }

            conf->font = NULL;
            conf->font = g_key_file_get_string ( keyFile, baseName,
                                                 "font", NULL);
        }
    }

    /* This is called in all GTK applications. Arguments are parsed
     * from the command line and are returned to the application. */
    gtk_init (&argc, &argv);

    /* for option processing. */
    char *optstring = "d:kmp:uvV";
    int opt = 0;

    while (( opt = getopt (argc, argv, optstring)) != -1) {
        switch (opt) {

        case 'd':
            switch (tolower (*optarg)) {
            case DEGREES:
                angle = DEGREES;
                break;

            case MINUTES:
                angle = MINUTES;
                break;

            case SECONDS:
                angle = SECONDS;
                break;

            default:
                (void) fprintf (stderr,
                                "\nDegrees must be one of 'd', ddd.dddddd;\n");
                (void) fprintf (stderr,
                                "'m', ddd mm.mmmm; or 's', dd mm ss.ss\n");
                return (-2);
            }
            break;

        case US:
            units = opt;
            break;

        case KNOTS:
            units = opt;
            break;

        case METRIC:
            units = opt;
            break;

        case 'p':
            (void) strncpy (hostPort, optarg, STRINGBUFFSIZE);
            break;

        case 'v':
            verbose = true;
            break;

        case 'V':       /* N.b.: falls through to the default case! */
            (void) fprintf (stderr,
                            "%s, compiled for gpsd API version %d.%d.\n",
                            versionString, GPSD_API_MAJOR_VERSION,
                            GPSD_API_MINOR_VERSION);

        default:        /* '?' */
            (void) fprintf(stderr,
                           "Usage: %s [-d d] [-m] [-k] [-p port] [-u] [-v] [-V] [host]\n",
                           baseName);
            return(-2);
        }
    }

    if (optind != argc) {
        /* we have a host name.... */
        (void) strncpy (hostName, argv[optind], STRINGBUFFSIZE);
    }

    {
        /* create a new window */
        window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

        /* Set our icon. */
        gtk_window_set_icon ((GtkWindow *)window,
                             gdk_pixbuf_new_from_inline ( -1, my_pixbuf,
                                                          false, NULL));

        /* When the window is given the "delete_event" signal (this is
         * given by the window manager, usually by the "close" option,
         * or on the titlebar), we ask it to call the delete_event ()
         * function as defined above. */
        g_signal_connect (G_OBJECT (window), "delete_event",
                          G_CALLBACK (delete_event), (gpointer) "Done");

        /* Here we connect the "destroy" event to a signal handler.
         * This event occurs when we call gtk_widget_destroy() on the
         * window, or if we return FALSE in the "delete_event"
         * callback. */
        g_signal_connect (G_OBJECT (window), "destroy",
                          G_CALLBACK (destroy), (gpointer) "Destroy");

        /* Sets the border width of the window. */
        gtk_container_set_border_width (GTK_CONTAINER (window), 1);

        /* And a title */
        (void) snprintf (titleBuff, STRINGBUFFSIZE,
                         "%s: a simple GTK+ GPS monitor", baseName);
        gtk_window_set_title (GTK_WINDOW (window), titleBuff);
    }

    /* Build a vertical box. Into it we will put the menu bar, and
     * then the table containing all the labels and entry widgets. */
    {
        vbox = gtk_vbox_new (FALSE, 1);
        gtk_container_set_border_width (GTK_CONTAINER (vbox), 1);
        gtk_container_add (GTK_CONTAINER (window), vbox);
    }

    /* Build and show the menu bar */
    {
        menubar = get_menubar_menu (window);
        gtk_box_pack_start (GTK_BOX (vbox), menubar, FALSE, TRUE, 0);
    }

    /* Build the table we'll stuff everything into. Heterogeneous
     * layout, please. */
    {
        table = gtk_table_new( ROWS, COLS, FALSE );

        /* Put the table in the vbox */
        gtk_container_add (GTK_CONTAINER (vbox), table);
    }

    /* Build pairs of widgets: a label, followed by a text entry
     * box. The table size is ROWS x COLS, based on a count of widgets
     * in the row. Co-ordinates here are for pairs, so the column
     * numbers are half what they would be for widgets. 0,0 is the
     * upper left, as with widgets. */

    buildPair (LAT,     "Lat",     table, 0, 0);
    buildPair (LONG,    "Long",    table, 1, 0);
    buildPair (SPEED,   "Speed",   table, 0, 1);
    buildPair (TRACK,   "Track",   table, 1, 1);
    buildPair (ALT,     "Alt",     table, 0, 2);
    buildPair (TIME,    "Time",    table, 1, 2);

    /* Set up the satellite progress bar. */
    progress = (GtkProgressBar *) gtk_progress_bar_new ();
    gtk_progress_bar_set_fraction (progress, 0.0);
    gtk_progress_bar_set_text (progress, "We've seen no data yet.");
    gtk_box_pack_end (GTK_BOX (vbox), GTK_WIDGET (progress), FALSE, TRUE, 0);

    if (conf->font != NULL && strlen (conf->font) > 0) {
        GtkRcStyle *style;

        style = gtk_rc_style_new ();
        pango_font_description_free (style->font_desc);
        style->font_desc = pango_font_description_from_string (conf->font);

        for ( i=0; i < SIZE; i++) {
            if (entries[i] != NULL) {
                gtk_widget_modify_style (GTK_WIDGET (entries[i]), style);
                /* Reset the width of the widget. */
            }
        }
        gtk_widget_modify_style (GTK_WIDGET (progress), style);
    }

    setColor (&NoFixColor);
    /* Always remember this step, this tells GTK that our preparation
     * for this window is complete, and it can now be displayed. */
    gtk_widget_show_all (window);

    setActiveAngle ();
    setActiveUnits ();

    /* (void) printf ("Host is %s, port is %s\n", host, port); */

    resynch ();

    /* Now set up our idle function, in milliseconds. */
    tag = g_timeout_add ((guint32)300, gpsPoll, NULL);

    /* All GTK applications must have a gtk_main(). Control ends here
     * and waits for an event to occur (like a key press or mouse
     * event). */
    gtk_main ();

    if (baseName != NULL) {
        g_free((gpointer) baseName);
    }

    if (keyFile != NULL) {
        g_free(keyFile);
    }

    return (ret);
}
