/* A GTK program for showing the latest gps data using libgps. */

/* License: This program is released under the same terms as gpsd
 * itself, i.e under the BSD License. See the file COPYING in the
 * distribution. This program comes with ABSOLUTELY NO WARRANTY. */

/* Prerequisites. Any fairly recent version should do.

   apt install gcc libgtk2.0-dev

*/

#include <gps.h>                /* Not the same as gpsd.h */

/* Do we recognize the current libgps API? We'll take 5.1, 6.1, 7.0,
 * or 8.0. If we correctly detect the version, we #define
 * VERSIONSET. If not, the last test in this sequence fails, and we
 * bomb out. */

#if ( GPSD_API_MAJOR_VERSION == 5 && GPSD_API_MINOR_VERSION == 1 )
#warning Setting up for version 5.1
#define VERSION501
#define VERSIONSET
#endif  /* 5.1 */

#if ( GPSD_API_MAJOR_VERSION == 6 && GPSD_API_MINOR_VERSION == 1 )
#warning Setting up for version 6.1
#define VERSION601
#define VERSIONSET
#endif  /* 6.1 */

#if ( GPSD_API_MAJOR_VERSION == 7 && GPSD_API_MINOR_VERSION == 0 )
#warning Setting up for version 7.0
#define VERSION700
#define VERSIONSET
#endif  /* 7.0 */

#if ( GPSD_API_MAJOR_VERSION == 8 && GPSD_API_MINOR_VERSION == 0 )
#warning Setting up for version 8.0
#define VERSION800
#define VERSIONSET
#endif  /* 8.0 */

#ifndef VERSIONSET
#error Unknown gps API protocol version; see gps.h for the current value of GPSD_API_MAJOR_VERSION
#endif  /* Unknown */

#if ( GPSD_API_MINOR_VERSION < 0 )
#error Unknown gps API protocol version; see gps.h for the current value of GPSD_API_MINOR_VERSION
#endif

#include <gtk/gtk.h>            /* "apt install libgtk2.0-dev" on
                                 * debian/ubuntu. */

#include <math.h>               /* For isnan (), modf (), fabs () */
#include <string.h>             /* For strcpy (), etc. */
#include <unistd.h>             /* getopt stuff */
#include <errno.h>
#include <ctype.h>              /* tolower () */
#include <sys/stat.h>           /* S_ISDIR () */

#include "gnome-gps.h"          /* prototypes and other goodies. */
#include "icon.image.h"         /* prototypes for the icon image. */

#include <dirent.h>             /* directory manipulation, dirent
                                 * stuff. */

#if GPSD_API_MAJOR_VERSION >= 7
/* from <gps_json.h> */
#define GPS_JSON_RESPONSE_MAX   4096

bool showMessage = false;
char gpsdMessage[GPS_JSON_RESPONSE_MAX];
size_t gpsdMessageLen = 0;
#endif

/* Settings that go into the configuration file. */
typedef struct {
    gchar *port, *host, *angle, *units, *font, *gmt;
} settings;

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
gboolean gmt = true;            /* Display time in GMT or local? */

gchar *fixBuffInit = "No fix seen yet";
gchar *timeStringInit = "No time yet";
gchar *latStringInit = "No latitude yet";
gchar *longStringInit = "No longitude yet";
gchar *altStringInit = "No altitude yet";
gchar *speedStringInit = "No speed yet";
gchar *trackStringInit = "No track yet";

gchar fixBuff[STRINGBUFFSIZE];
gchar timeString[STRINGBUFFSIZE];
gchar latString[STRINGBUFFSIZE];
gchar longString[STRINGBUFFSIZE];
gchar altString[STRINGBUFFSIZE];
gchar speedString[STRINGBUFFSIZE];
gchar trackString[STRINGBUFFSIZE];

gchar keyFileName[STRINGBUFFSIZE];
gchar titleBuff[STRINGBUFFSIZE]; /* the title in the title bar */

/* Defaults, which the user may change later on. */
char hostName[STRINGBUFFSIZE] = "localhost";
char hostPort[STRINGBUFFSIZE] = DEFAULT_GPSD_PORT;

/* A place for functions to put return strings. */
char returnString[STRINGBUFFSIZE];

const gchar *baseName;          /* So we can print out only the base
                                   name in the help. */

/* SIZE must be last as we use it to size the array and in related
 * code. */
enum entryNames { LAT, LONG, ALT, SPEED, TRACK,
                  TIME, SIZE
                } entryNames;

GtkEntry *entries[SIZE];

inline void preserve (GtkEntry *entry, gint index) {
    entries [index] = entry;
}

/* Some colors for various fix states */
/*                        pixel, red,  green,   blue. */
GdkColor ThreeDFixColor = {0,      0, 0xffff,      0}; /* green */
GdkColor TwoDFixColor   = {0, 0xff00, 0xff00,      0}; /* yellow */
GdkColor NoFixColor     = {0, 0xff00,      0,      0}; /* red */
GdkColor NoGpsdColor    = {0, 0x8000, 0x8000, 0x8000}; /* grey */
GdkColor *oldColor = NULL;

/* The name of the configuration directory. If it exists, in the home
 * directory, we'll create and look for our configuration files
 * here. */

char *configDir = ".config";

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

/* Initialize our string buffers as we come up, and again on
 * resync. Use strcpy rather than strncpy because thes are all known
 * sizes and locations. Not much chance of a buffer overflow,
 * malicious or otherwise. */
void initStrings (void) {
    (void) strcpy (fixBuff, fixBuffInit);
    (void) strcpy (timeString, timeStringInit);
    (void) strcpy (latString, latStringInit);
    (void) strcpy (longString, longStringInit);
    (void) strcpy (altString, altStringInit);
    (void) strcpy (speedString, speedStringInit);
    (void) strcpy (trackString, trackStringInit);

    gtk_entry_set_text(entries[TIME], timeString );
    gtk_entry_set_text(entries[LAT], latString );
    gtk_entry_set_text(entries[LONG], longString );
    gtk_entry_set_text(entries[ALT], altString );
    gtk_entry_set_text(entries[SPEED], speedString );
    gtk_entry_set_text(entries[TRACK], trackString );
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
    (void) strncat (latString, latitude < 0.0 ? "S" : latitude > 0.0 ? "N" : "",
                    latitude == 0.0 ? 1 : 2);
    gtk_entry_set_text(entries[LAT], latString );
}

void formatLong (double longitude) {
    double longAbs = fabs (longitude);
    formatAngle (longAbs, longString);
    (void) strncat (longString, longitude < 0.0 ? "W" : longitude > 0.0 ? "E" : "",
                    longitude == 0.0 ? 1 : 2);
    gtk_entry_set_text(entries[LONG], longString );
}

char *gnome_gps_unix_to_iso8601(timestamp_t fixtime,
                                char isotime[], size_t len)
/* Unix time to ISO8601. Filched from gpsd's gpsutils.c. example:
 * 2007-12-11T23:38:51.033 */
{
    struct tm when;
    double integral, fractional;
    time_t intfixtime;
    char timestr[30];
    char fractstr[10];

    fractional = modf(fixtime, &integral);
    intfixtime = (time_t) integral;
    (void)localtime_r(&intfixtime, &when);

    (void)strftime(timestr, sizeof(timestr), "%Y-%m-%dT%H:%M:%S", &when);
    /*
     * Do not mess casually with the number of decimal digits in the
     * format!  Most GPSes report over serial links at 0.01s or 0.001s
     * precision.
     */
    (void)snprintf(fractstr, sizeof(fractstr), "%.3f", fractional);
    /* add fractional part, ignore leading 0; "0.2" -> ".2" */
    /*@i2@*/(void)snprintf(isotime, len, "%s%s",timestr, strchr(fractstr,'.'));
    return isotime;
}

void formatTime (double time) {
    if (isnan(time)==0) {
        if (gmt == true) {
            (void) unix_to_iso8601(time, timeString,
                                   (int) sizeof(timeString));
        } else {
            (void) gnome_gps_unix_to_iso8601(time, timeString,
                                             (int) sizeof(timeString));
        }
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

static void resynch (void) {
    int ret = 0;

    setColor (&NoGpsdColor);

    if (haveConnection == true) {
        (void) gps_stream(&gpsdata, WATCH_DISABLE, NULL);
        (void) gps_close (&gpsdata);
        haveConnection = false;
    }

    gtk_progress_bar_set_fraction (progress, 0.0);
    initStrings ();
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
        gtk_progress_bar_set_text (progress, "Synch Failure: No gpsd connection!");
        haveConnection = false;
    } else {
        /* If we got here, we're good to go. */
        gtk_progress_bar_set_text (progress, "Ahhh, a gpsd connection!");
        gpsLost = false;
        haveConnection = true;
        sendWatch ();
    }
}

/* This exists so we can call resynch () from the menu. We don't
 * actually use the parameters at all. We call resynch () elsewhere in
 * the program. */
static void resynchWrapper ( gpointer   callback_data,
                             guint      callback_action,
                             GtkWidget *menu_item ) {
    resynch ();
}

static void hostOkCallback (GtkWidget *widget,
                            gpointer   data) {
    const gchar *sandbox = NULL; /* look but don't touch */

    /* Get the contents of the two entry widgets */
    sandbox = gtk_entry_get_text (GTK_ENTRY (hostEntry));

    if ((sandbox != NULL) && (strlen (sandbox) < STRINGBUFFSIZE) && (strlen (sandbox))) {
        (void) strncpy (hostName, sandbox, STRINGBUFFSIZE-1);
    }

    sandbox = gtk_entry_get_text (GTK_ENTRY (portEntry));
    if ((sandbox != NULL) && (strlen (sandbox) < STRINGBUFFSIZE) && (strlen (sandbox))) {
        (void) strncpy (hostPort, sandbox, STRINGBUFFSIZE-1);
    }

    gtk_widget_destroy (hostDialogBox);

    /* go ahead and implement the change, if any. */
    resynch ();
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
gchar commentLine[STRINGBUFFSIZE];

static void aboutDialog( gpointer   callback_data,
                         guint      callback_action,
                         GtkWidget *menu_item ) {
    (void) snprintf (commentLine, sizeof (commentLine),
                     "A simple GTK+ GPS monitor.\n\n"
                     "Grey: No GPS daemon.\nRed: No fix.\n"
                     "Yellow: Two dimensional fix.\nGreen: Three dimensional fix.\n"
                     "\"Save\" to save settings such as font and units.\n\n"
                     "libgps API version %d.%d.\n%s version %s.",
                     GPSD_API_MAJOR_VERSION, GPSD_API_MINOR_VERSION,
                     baseName, versionString);

    gtk_show_about_dialog (GTK_WINDOW (window),
                           "authors", authors,
                           "comments", commentLine,
                           "copyright", copyrightString,
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
    { "/Units/Sep",                  NULL,        NULL,       0, "<Separator>"          },
    { "/Units/_Gmt",            "<ctrl>g",      setGmt,    true, "<RadioItem>"          },
    { "/Units/_Local",          "<ctrl>l",      setGmt,   false, "/Units/Gmt"           },
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

static void setGmt( gpointer   callback_data,
                    guint      callback_action,
                    GtkWidget *menu_item ) {
    gmt = callback_action;
}


static void setDegrees( gpointer   callback_data,
                        guint      callback_action,
                        GtkWidget *menu_item ) {
    angle = callback_action;
}

static void saveState( gpointer   callback_data,
                       guint      callback_action,
                       GtkWidget *menu_item ) {
    gchar *results;
    if (haveHome) {
        results = saveKeyFile (keyFile);
        gtk_progress_bar_set_text (progress, results);
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

#if GPSD_API_MAJOR_VERSION >= 7
    if (showMessage == true) {
        int len;
        len = strlen (gpsdMessage);
        if ( '\r' == gpsdMessage[len - 1]) {
            /* remove any trailing \r */
            gpsdMessage[len - 1] = '\0';
        }
        printf ("%s\n", gpsdMessage);
    }
#endif

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
            setColor (&NoGpsdColor);

            if (verbose) {
                (void) fprintf (stderr, "gps lost.\n");
                (void) snprintf(tmpBuff, sizeof(tmpBuff),
                                "driver = %s: subtype = %s: activated = %f",
                                gpsdata.dev.driver, gpsdata.dev.subtype,
                                gpsdata.dev.activated);
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
        gpsdata.set &= ~(DEVICELIST_SET);
        printf ("set 0x%08x, %d device%s found.\n",
                (int) gpsdata.set,
                gpsdata.devices.ndevices,
                gpsdata.devices.ndevices == 1 ? "" : "s");
        if (gpsdata.devices.ndevices >= 1) {
            /* N.B: I have tested this with one device, but have not
             * tested it with multiple devices. */
            int i;

            for (i = 0 ; i < gpsdata.devices.ndevices; i++ ) {
                (void) printf("Device no. %i: driver = %s: subtype (if any) = %s\n",
                              i,
                              gpsdata.devices.list[0].driver,
                              gpsdata.devices.list[0].subtype);
            }
        } else {
            (void) printf ("set 0x%08x, no devices reported.\n",
                           (unsigned int) gpsdata.set);
        }
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
            /* We have at least one satellite visible and at least one
             * used in the fix. */
            /* (void) printf ("Yes, we see %2d satellites and use %2d.\n", */
            /*                gpsdata.satellites_visible, */
            /*                gpsdata.satellites_used); */
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
                /* We have at least one satellite visible and none used in
                 * the fix. */
                /* (void) printf ("We see %2d satellites.\n", */
                /*                gpsdata.satellites_visible); */
                (void) snprintf (banner, STRINGBUFFSIZE,
                                 "%2d satellites visible, no fix.",
                                 gpsdata.satellites_visible);
            } else {
                /* No satellites visible, no fix. */
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

        /* This ifdef was obvioulsy incorrect! */
        /* #ifdef VERSION501 */
        case STATUS_DGPS_FIX:
            /* #endif */

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
 * box. N.B.: This routine does not intialize the strings for the
 * widgets. Do that in the calling function with initStrings. */
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
#if GPSD_API_MAJOR_VERSION >= 7 /* API change. */
        if (gps_read (&gpsdata, gpsdMessage, gpsdMessageLen) == -1) {
#else
        if (gps_read (&gpsdata) == -1) {
#endif
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
gchar *saveKeyFile (GKeyFile *keyFile) {
    FILE *file;
    gchar *string = NULL;
    gsize length = 0;
    gsize size = 0;
    gchar sandbox[128];
    GtkStyle *style;
    const gchar *fontName;
    int ret = 0;                /* for return values from functions. */

    if (haveHome == false) {
        return ("No home file to save to!");
    }

    g_key_file_set_value (keyFile, baseName, "host", hostName);
    g_key_file_set_value (keyFile, baseName, "port", hostPort);

    sandbox [1] = '\0';
    sandbox [0] = angle;
    g_key_file_set_value (keyFile, baseName, "angle", sandbox);

    sandbox [0] = units;
    g_key_file_set_value (keyFile, baseName, "units", sandbox);

    sandbox [0] = gmt ? 't' : 'f';
    g_key_file_set_value (keyFile, baseName, "gmt", sandbox);

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
            (void) snprintf (returnString, STRINGBUFFSIZE,
                             "Could not open file '%s'.\n",
                             keyFileName);
            fprintf (stderr, returnString);
            return (returnString);
        }
        size = fwrite (string, length, 1, file);
        if (size != 1) {
            (void) snprintf (returnString, STRINGBUFFSIZE,
                             "Failure writing to file '%s'.\n",
                             keyFileName);
            fprintf (stderr, returnString);
            return (returnString);
        }
        ret = fclose (file);
        if (ret == EOF) {
            (void) snprintf (returnString, STRINGBUFFSIZE,
                             "Failure closing file '%s'. %s\n",
                             keyFileName, strerror(errno));
            fprintf (stderr, returnString);
            return (returnString);
        }
        (void) snprintf (returnString, STRINGBUFFSIZE,
                         "Settings saved to file '%s'.",
                         keyFileName);
        return (returnString);
    } else {
        return ("Failure gathering parameters to save.");
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

void setActiveGmt (void) {
    switch (gmt) {

    /* Use the default to silently handle errors and fall through
     * to the substitute case. */
    default:
        gmt = true;
    case true:
        gtk_check_menu_item_set_active(
            GTK_CHECK_MENU_ITEM (gtk_item_factory_get_item (item_factory, "/Units/Gmt")),
            TRUE);
        break;

    case false:
        gtk_check_menu_item_set_active(
            GTK_CHECK_MENU_ITEM (gtk_item_factory_get_item (item_factory, "/Units/Local")),
            TRUE);
        break;
    }
}

/* A filter for scanning the main directory. Return true if the target
 * directory exists. */
int filter(const struct dirent *entry) {
    int ret = (strcmp(entry->d_name, configDir) == 0);

    /* No point in testing to be sure it's a directory if the name is
     * wrong. */
    if (ret == FALSE) return (ret);

    /* This is a convoluted mess. We have to convert from the values
     * returned in the directory entry to the types used in struct
     * stat (man 2 stat) so we can test to see if it is a
     * directory. */
    ret = S_ISDIR (DTTOIF (entry->d_type));

    return (ret);
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

    (void) printf ("\n%s: %s", baseName, copyrightString);

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

            (void) strncpy (keyFileName, home, STRINGBUFFSIZE-1);

            /* If the ~/.config directory exists, use it. */
            {

                struct dirent **namelist;
                int n = 0;

                n = scandir(keyFileName, &namelist, filter, alphasort);
                if (n == -1) {
                    perror("scandir");
                } else {

                    if (n > 0) {
                        /* We found our target, so add the last one found to the directory */
                        (void) strncat (keyFileName, "/", (STRINGBUFFSIZE-1) - strlen (keyFileName));
                        (void) strncat (keyFileName, namelist[n-1]->d_name, (STRINGBUFFSIZE-1) - strlen (keyFileName));
                        (void) strncat (keyFileName, "/", (STRINGBUFFSIZE-1) - strlen (keyFileName));

                        while (n--) {
                            g_free(namelist[n]);
                        }
                    } else {
                        /* If we didn't find any targets, first add a
                         * slash, then a period because it's a hidden
                         * file. */
                        (void) strncat (keyFileName, "/.", (STRINGBUFFSIZE-1) - strlen (keyFileName));
                    }
                    g_free(namelist);
                }
            }

            (void) strncat (keyFileName, baseName,
                            (STRINGBUFFSIZE-1) - strlen (keyFileName));

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
                (void) strncpy (hostName, conf->host, STRINGBUFFSIZE-1);
            }

            conf->port = g_key_file_get_string ( keyFile, baseName,
                                                 "port", NULL);
            if (conf->port != NULL && strlen (conf->port) > 0) {
                (void) strncpy (hostPort, conf->port, STRINGBUFFSIZE-1);
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

            conf->gmt = NULL;
            conf->gmt = g_key_file_get_string ( keyFile, baseName,
                                                "gmt", NULL);
            if (conf->gmt != NULL && strlen (conf->units) > 0) {
                gmt = (conf->gmt[0] == 't') ? true : false;
            }

        }
    }

    /* This is called in all GTK applications. Arguments are parsed
     * from the command line and are returned to the application. */
    gtk_init (&argc, &argv);

    /* for option processing. */
#if GPSD_API_MAJOR_VERSION >= 7 /* API change. */
    char *optstring = "d:jkmp:uvV";
#else
    char *optstring = "d:kmp:uvV";
#endif
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

#if GPSD_API_MAJOR_VERSION >= 7 /* API change. */
        case 'j':
            showMessage = true;
            gpsdMessageLen = GPS_JSON_RESPONSE_MAX;
            break;
#endif

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
            (void) strncpy (hostPort, optarg, STRINGBUFFSIZE-1);
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
#if GPSD_API_MAJOR_VERSION >= 7 /* API change. */
            (void) fprintf(stderr,
                           "Usage: %s [-d d] [-j] [-m] [-k] [-p port] [-u] [-v] [-V] [host]\n",
                           baseName);
#else
            (void) fprintf(stderr,
                           "Usage: %s [-d d] [-m] [-k] [-p port] [-u] [-v] [-V] [host]\n",
                           baseName);
#endif
            return(-2);
        }
    }

    if (optind != argc) {
        /* we have a host name.... */
        (void) strncpy (hostName, argv[optind], STRINGBUFFSIZE-1);
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

    initStrings ();

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

    /* setColor (&NoGpsdColor); */

    /* Always remember this step, this tells GTK that our preparation
     * for this window is complete, and it can now be displayed. */
    gtk_widget_show_all (window);

    setActiveAngle ();
    setActiveUnits ();
    setActiveGmt ();

    /* (void) printf ("Host is %s, port is %s\n", host, port); */

    /* just in case... */
    gpsdata.set = 0;

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
