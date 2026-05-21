/* A GTK program for showing the latest gps data using libgps. */

/* License: This program is released under the same terms as gpsd
 * itself, i.e under the BSD License. See the file COPYING in the
 * distribution. This program comes with ABSOLUTELY NO WARRANTY. */

/* Prerequisites. Any fairly recent version should do.

   apt install gcc libgtk2.0-dev

*/

/* Version 2.0 greatly benefited from
 * https://gpsd.io/gpsd-client-example-code.html. */

#include <gps.h>                /* Not the same as gpsd.h */

/* Do we recognize the current libgps API? We'll take 10.0, 11.0, and
 * 12.0. Note that we skipped 9.1. If we correctly detect the version,
 * we #define VERSIONSET. If not, the last test in this sequence
 * fails, and we bomb out. If you insist on using a prior version, see
 * previous versions of this code by checking them out from the git
 * repository. */

/* Skipping 9.1. See /usr/include/gps.h or /usr/local/include/gps.h. */
#if ( GPSD_API_MAJOR_VERSION == 10 && GPSD_API_MINOR_VERSION == 0 )
#warning Setting up for API version 10.0
#define VERSION1000
#define VERSIONSET
#endif  /* 10.0 */

/* This is for the version on Debian 10, Buster, gpsd version 3.22, libgps version 28. */
#if ( GPSD_API_MAJOR_VERSION == 11 && GPSD_API_MINOR_VERSION == 0 )
#warning Setting up for API version 11.0
#define VERSION1100
#define VERSIONSET
#endif  /* 11.0 */

/* This is for the version on Debian 10, Buster, gpsd version 3.23.1~rc1, libgps version 29. */
#if ( GPSD_API_MAJOR_VERSION == 12 && GPSD_API_MINOR_VERSION == 0 )
#warning Setting up for API version 12.0
#define VERSION1200
#define VERSIONSET
#endif  /* 12.0 */

/* This is for the version on Debian 13, trixie & up, gpsd version 3.25.1~dev, libgps version ??. */
#if ( GPSD_API_MAJOR_VERSION == 14 && GPSD_API_MINOR_VERSION == 0 )
#warning Setting up for API version 14.0
#define VERSION1400
#define VERSIONSET
#endif  /* 14.0 */

/* This is for the version on Debian 13, trixie & up, gpsd version 3.27, libgps version ??. */
#if GPSD_API_MAJOR_VERSION == 16
#if GPSD_API_MINOR_VERSION == 0
#warning Setting up for API version 16.0
#define VERSION1600
#endif  /* 16.0 */
#if GPSD_API_MINOR_VERSION == 1 /* 16.1 */
#warning Setting up for API version 16.1
#define VERSION1601
#endif  /* 16.1 */
#define VERSIONSET
#endif  /* 16.0 or 16.1 */

#ifndef VERSIONSET
#error Unknown gps API protocol version; see gps.h for the current value of GPSD_API_MAJOR_VERSION
#endif  /* Unknown */

#if ( GPSD_API_MINOR_VERSION < 0 )
#error Unknown gps API protocol version; see gps.h for the current value of GPSD_API_MINOR_VERSION
#endif


#include <math.h>               /* For isnan (), modf (), fabs () */
#include <string.h>             /* For strcpy (), etc. */
#include <unistd.h>             /* getopt stuff */
#include <errno.h>
#include <ctype.h>              /* tolower () */
#include <sys/stat.h>           /* S_ISDIR () */

#include "gnome-gps.h"          /* prototypes and other goodies. */

#include <dirent.h>             /* directory manipulation, dirent
                                 * stuff. */

/* Settings that go into the configuration file. */
typedef struct {
    gchar *port, *host, *angle, *units, *font, *gmt, *magnetic;
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
GtkCssProvider *colorProvider = NULL; /* fix-state background colors */
GtkCssProvider *fontProvider  = NULL; /* user-selected display font */
PangoFontDescription *currentFontDesc = NULL; /* the font currently in use */
gchar *initialFont = NULL;      /* font from the config file (applied at startup) */

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
gboolean magnetic = false;      /* Display true or magnetic heading? */

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

/* GTK4 moved the text setter onto GtkEditable; this keeps the calling
 * code below unchanged. */
static inline void setEntryText (GtkEntry *entry, const char *text) {
    gtk_editable_set_text (GTK_EDITABLE (entry), text);
}

/* Fix-state background colors are CSS classes (gps-3d, gps-2d, no-fix,
 * no-gpsd) loaded into colorProvider in on_activate (); setColor ()
 * swaps the class on the root container. */

/* The name of the configuration directory. If it exists, in the home
 * directory, we'll create and look for our configuration files
 * here. */

char *configDir = ".config";

/* A string array to make the status more human friendly. Indexed by
 * the various status in gpsd.h. With boundaries so we can handle
 * unknown values. */
static char statusString[SIZESTATUSSTRINGS][19]
    = {"unknown", "no", "", "DGPS",
       "RTK fixed", "RTK float", "Dead reckoning",
       "GNSS dead reckoning", "Time only",
       "Simulated", "PPS", "unknown"
      };

/* getStatusString: Get a string appropriate for the current
 * status. With boundary checking. */

char *getStatusString (int status) {
    status++;
    status = MIN (status, SIZESTATUSSTRINGS);
    status = MAX (status, 0);
    return statusString[status];
}

/* Similarly, a string array to make the mode more human
 * friendly. Indexed by the various modes in gpsd.h. */
static char modeString[SIZEMODESTRINGS][9]
    = {"unseen", "no fix", "2D fix", "3D fix",};

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
 * resync. Use strcpy rather than strncpy because these are all known
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

    setEntryText(entries[TIME], timeString );
    setEntryText(entries[LAT], latString );
    setEntryText(entries[LONG], longString );
    setEntryText(entries[ALT], altString );
    setEntryText(entries[SPEED], speedString );
    setEntryText(entries[TRACK], trackString );
}

/* We have to change the background of the window (which gets the
 * labels, since they don't have their own backgrounds), and the
 * individual text entry widgets, which do have their own
 * backgrounds. */
void setColor (const char *cssClass) {
    static const char *cur = NULL;

    /* The call sites pass string literals, so a pointer compare is
     * enough (and mirrors the old "color != oldColor" guard). */
    if (cur == cssClass) {
        return;
    }

    if (cur != NULL) {
        gtk_widget_remove_css_class (vbox, cur);
    }
    gtk_widget_add_css_class (vbox, cssClass);
    cur = cssClass;
}

/* Tear down the gpsd connection and stop polling. Idempotent, so it is
 * safe to call from both the window's close-request handler and the
 * quit action. */
void cleanup_gps (void) {
    if (haveConnection == true) {
        (void) gps_stream(&gpsdata, WATCH_DISABLE, NULL);
        (void) gps_close (&gpsdata);
        haveConnection = false;
    }

    if (tag != 0) {
        g_source_remove (tag);
        tag = 0;
    }
}

/* GTK4 replacement for the old delete_event/destroy pair. The
 * application exits when its last window closes; returning FALSE lets
 * the close proceed. */
static gboolean on_close_request (GtkWindow *self,
                                  gpointer   data) {
    cleanup_gps ();
    return FALSE;
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
                     "%03.0f° %s %s", track,
                     dirString,
                     magnetic ? "Magnetic" : "True");
    setEntryText(entries[TRACK], trackString );
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
    setEntryText(entries[LAT], latString );
}

void formatLong (double longitude) {
    double longAbs = fabs (longitude);
    formatAngle (longAbs, longString);
    (void) strncat (longString, longitude < 0.0 ? "W" : longitude > 0.0 ? "E" : "",
                    longitude == 0.0 ? 1 : 2);
    setEntryText(entries[LONG], longString );
}

char *gnome_gps_timespec_to_iso8601(timespec_t fixtime, char isotime[], size_t len)
/* Filched from gpsd's gpsutils.c. */
/* timespec UTC time to ISO8601, no timezone adjustment. example:
 * 2007-12-11T23:38:51.033Z */
{
    struct tm when;
    char timestr[30];
    long fracsec;

    if (0 > fixtime.tv_sec) {
        // Allow 0 for testing of 1970-01-01T00:00:00.000Z
        return strncpy(isotime, "NaN", len);
    }
    if (999499999 < fixtime.tv_nsec) {
        /* round up */
        fixtime.tv_sec++;
        fixtime.tv_nsec = 0;
    }

    (void)localtime_r(&fixtime.tv_sec, &when);

    /*
     * Do not mess casually with the number of decimal digits in the
     * format!  Most GPSes report over serial links at 0.01s or 0.001s
     * precision.  Round to 0.001s
     */
    fracsec = (fixtime.tv_nsec + 500000) / 1000000;

    (void)strftime(timestr, sizeof(timestr), "%Y-%m-%dT%H:%M:%S", &when);
    (void)snprintf(isotime, len, "%s.%03ld", timestr, fracsec);

    return isotime;
}

void formatTime (timespec_t time) {
    if (time.tv_sec > 0) {
        if (gmt == true) {
            (void) timespec_to_iso8601(time, timeString,
                                       (int) sizeof(timeString));
        } else {
            (void) gnome_gps_timespec_to_iso8601(time, timeString,
                                                 (int) sizeof(timeString));
        }
    } else {
        (void) strcpy(timeString,"n/a");
    }
    setEntryText(entries[TIME], timeString );
}

void formatAltitude (double altitude) {
    if (units != METRIC) {
        altitude *= METERS_TO_FEET;
    }

    (void) snprintf (altString, STRINGBUFFSIZE,
                     units != METRIC ? "%.0f feet" : "%.1f meters",
                     altitude);
    setEntryText(entries[ALT], altString );
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
    setEntryText(entries[SPEED], speedString );
}

static void resynch (void) {
    int ret = 0;

    setColor ("no-gpsd");

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
        gtk_progress_bar_set_text (progress, "Sync Failure: No gpsd connection!");
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
static void resynchAction ( GSimpleAction *action,
                            GVariant      *parameter,
                            gpointer       data ) {
    resynch ();
}

static void hostOkCallback (GtkWidget *widget,
                            gpointer   data) {
    const gchar *sandbox = NULL; /* look but don't touch */

    /* Get the contents of the two entry widgets */
    sandbox = gtk_editable_get_text (GTK_EDITABLE (hostEntry));

    if ((sandbox != NULL) && (strlen (sandbox) < STRINGBUFFSIZE) && (strlen (sandbox))) {
        (void) strncpy (hostName, sandbox, STRINGBUFFSIZE-1);
    }

    sandbox = gtk_editable_get_text (GTK_EDITABLE (portEntry));
    if ((sandbox != NULL) && (strlen (sandbox) < STRINGBUFFSIZE) && (strlen (sandbox))) {
        (void) strncpy (hostPort, sandbox, STRINGBUFFSIZE-1);
    }

    gtk_window_destroy (GTK_WINDOW (hostDialogBox));

    /* go ahead and implement the change, if any. */
    resynch ();
}

static void hostCancelCallback (GtkWidget *widget,
                                gpointer   data) {
    gtk_window_destroy (GTK_WINDOW (hostDialogBox));
}

/* When the host/port window goes away -- via OK, Cancel, or the window
 * manager -- forget its widgets so the single-instance guard in
 * hostAction () lets a later request build a fresh dialog instead of
 * presenting (or touching) freed pointers. */
static void hostDestroyCallback (GtkWidget *widget,
                                 gpointer   data) {
    hostDialogBox = NULL;
    hostEntry = NULL;
    portEntry = NULL;
}

/* Build one right-aligned label plus an editable entry row in the
 * dialog's grid, and remember the entry via *entry. */
static void hostPair (GtkWidget *grid, gint row, const gchar *labelText,
                      GtkWidget **entry, const gchar *value) {
    GtkWidget *child;

    child = gtk_label_new (labelText);
    gtk_widget_set_halign (child, GTK_ALIGN_END);
    gtk_grid_attach (GTK_GRID (grid), child, 0, row, 1, 1);

    *entry = gtk_entry_new ();
    gtk_editable_set_editable (GTK_EDITABLE (*entry), true);
    gtk_editable_set_text (GTK_EDITABLE (*entry), value);
    gtk_widget_set_hexpand (*entry, TRUE);
    g_signal_connect (*entry, "activate",
                      G_CALLBACK (hostOkCallback), NULL);
    gtk_grid_attach (GTK_GRID (grid), *entry, 1, row, 1, 1);
}

static void hostAction( GSimpleAction *action,
                        GVariant      *parameter,
                        gpointer       data ) {
    GtkWidget *box;
    GtkWidget *grid;
    GtkWidget *buttonBox;
    GtkWidget *okButton;
    GtkWidget *cancelButton;

    /* Only one host/port dialog at a time: re-present the existing one. */
    if (hostDialogBox != NULL) {
        gtk_window_present (GTK_WINDOW (hostDialogBox));
        return;
    }

    /* GTK4 has no gtk_dialog_run (); build a plain modal window and
     * drive it with the button/activate callbacks. */
    hostDialogBox = gtk_window_new ();
    gtk_window_set_title (GTK_WINDOW (hostDialogBox), "Host");
    gtk_window_set_transient_for (GTK_WINDOW (hostDialogBox), GTK_WINDOW (window));
    gtk_window_set_modal (GTK_WINDOW (hostDialogBox), TRUE);
    gtk_window_set_destroy_with_parent (GTK_WINDOW (hostDialogBox), TRUE);
    g_signal_connect (hostDialogBox, "destroy",
                      G_CALLBACK (hostDestroyCallback), NULL);

    box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_margin_start  (box, 5);
    gtk_widget_set_margin_end    (box, 5);
    gtk_widget_set_margin_top    (box, 5);
    gtk_widget_set_margin_bottom (box, 5);
    gtk_window_set_child (GTK_WINDOW (hostDialogBox), box);

    grid = gtk_grid_new ();
    gtk_grid_set_row_spacing (GTK_GRID (grid), YPADDING);
    gtk_grid_set_column_spacing (GTK_GRID (grid), XPADDING);
    gtk_box_append (GTK_BOX (box), grid);

    hostPair (grid, 0, "Host", &hostEntry, hostName);
    hostPair (grid, 1, "Port", &portEntry, hostPort);

    buttonBox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_widget_set_halign (buttonBox, GTK_ALIGN_END);
    gtk_box_append (GTK_BOX (box), buttonBox);

    okButton = gtk_button_new_with_label ("OK");
    g_signal_connect (okButton, "clicked",
                      G_CALLBACK (hostOkCallback), NULL);
    gtk_box_append (GTK_BOX (buttonBox), okButton);

    cancelButton = gtk_button_new_with_label ("Cancel");
    g_signal_connect (cancelButton, "clicked",
                      G_CALLBACK (hostCancelCallback), NULL);
    gtk_box_append (GTK_BOX (buttonBox), cancelButton);

    gtk_window_present (GTK_WINDOW (hostDialogBox));
}

/* Some properties string arrays for the About dialog. */
gchar *authors[2] =     { "Charles Curley, http://www.charlescurley.com/", NULL };
gchar *documentors[2] = { "Charles Curley, http://www.charlescurley.com/", NULL };
gchar commentLine[STRINGBUFFSIZE];

static void aboutAction( GSimpleAction *action,
                         GVariant      *parameter,
                         gpointer       data ) {
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

/* The menu is built as a GMenu model in buildMenuModel (); the actions
 * it refers to are registered in setupActions (). */

/* Now some functions for the menu. The mutually-exclusive groups are
 * string-state radio actions; each change-state handler stashes the
 * chosen value in the C global the rest of the program already uses. */
static void unitsChangeState( GSimpleAction *action,
                              GVariant      *value,
                              gpointer       data ) {
    units = g_variant_get_string (value, NULL)[0];
    g_simple_action_set_state (action, value);
}

static void angleChangeState( GSimpleAction *action,
                              GVariant      *value,
                              gpointer       data ) {
    angle = g_variant_get_string (value, NULL)[0];
    g_simple_action_set_state (action, value);
}

static void gmtChangeState( GSimpleAction *action,
                            GVariant      *value,
                            gpointer       data ) {
    gmt = (g_variant_get_string (value, NULL)[0] == 'g');
    g_simple_action_set_state (action, value);
}

static void magneticChangeState( GSimpleAction *action,
                                 GVariant      *value,
                                 gpointer       data ) {
    magnetic = (g_variant_get_string (value, NULL)[0] == 'm');
    g_simple_action_set_state (action, value);
}

static void saveAction( GSimpleAction *action,
                        GVariant      *parameter,
                        gpointer       data ) {
    gchar *results;
    if (haveHome) {
        results = saveKeyFile (keyFile);
        gtk_progress_bar_set_text (progress, results);
    }
}

static void quitAction( GSimpleAction *action,
                        GVariant      *parameter,
                        gpointer       data ) {
    cleanup_gps ();
    gtk_window_close (GTK_WINDOW (window));
}

/* CSS-escape a string for use inside a double-quoted CSS string
 * (escape backslash and double-quote). The caller frees the result.
 * Without this a font family name containing a quote or backslash --
 * e.g. from a hand-edited config -- could break out of, or inject
 * into, the generated font CSS. */
static gchar *cssEscapeString (const char *s) {
    GString *out = g_string_new (NULL);

    for (; s != NULL && *s != '\0'; s++) {
        if (*s == '\\' || *s == '"') {
            g_string_append_c (out, '\\');
        }
        g_string_append_c (out, *s);
    }

    return g_string_free (out, FALSE);
}

/* Build a CSS fragment that sets the display font, scoped under the
 * permanent .gnome-gps-ui class so the host dialog keeps the default
 * font. The caller frees the result. */
static gchar *pangoDescToCss (PangoFontDescription *desc) {
    const char *family = pango_font_description_get_family (desc);
    gint size = pango_font_description_get_size (desc);
    gboolean absolute = pango_font_description_get_size_is_absolute (desc);
    PangoWeight weight = pango_font_description_get_weight (desc);
    PangoStyle pstyle = pango_font_description_get_style (desc);
    gchar *familyCss;
    gchar *sizeCss;
    gchar *css;

    if (size <= 0) {
        sizeCss = g_strdup ("");
    } else if (absolute) {
        sizeCss = g_strdup_printf ("font-size: %dpx;", size / PANGO_SCALE);
    } else {
        sizeCss = g_strdup_printf ("font-size: %dpt;", size / PANGO_SCALE);
    }

    familyCss = cssEscapeString (family != NULL ? family : "Sans");
    css = g_strdup_printf (
              ".gnome-gps-ui entry, .gnome-gps-ui entry > text,"
              " .gnome-gps-ui progressbar > text {"
              " font-family: \"%s\"; %s"
              " font-weight: %d; font-style: %s; }",
              familyCss,
              sizeCss,
              (int) weight,
              pstyle == PANGO_STYLE_ITALIC ? "italic" :
              pstyle == PANGO_STYLE_OBLIQUE ? "oblique" : "normal");

    g_free (familyCss);
    g_free (sizeCss);
    return css;
}

/* (Re)load the font provider from currentFontDesc. */
static void applyFontCss (void) {
    gchar *css;

    if (currentFontDesc == NULL || fontProvider == NULL) {
        return;
    }

    css = pangoDescToCss (currentFontDesc);
    gtk_css_provider_load_from_data (fontProvider, css, -1);
    g_free (css);
}

/* Async finish for the font dialog. The source object is the
 * GtkFontDialog, which we own and must unref here. */
static void onFontChosen (GObject      *source,
                          GAsyncResult *res,
                          gpointer      data) {
    PangoFontDescription *desc;

    desc = gtk_font_dialog_choose_font_finish (GTK_FONT_DIALOG (source),
           res, NULL);
    if (desc != NULL) {
        if (currentFontDesc != NULL) {
            pango_font_description_free (currentFontDesc);
        }
        currentFontDesc = desc;
        applyFontCss ();
    }

    g_object_unref (source);
}

static void fontAction( GSimpleAction *action,
                        GVariant      *parameter,
                        gpointer       data ) {
    GtkFontDialog *dialog;
    gchar title[STRINGBUFFSIZE];

    (void) snprintf (title, STRINGBUFFSIZE, "%s: Select a font", baseName);

    dialog = gtk_font_dialog_new ();
    gtk_font_dialog_set_title (dialog, title);
    gtk_font_dialog_choose_font (dialog, GTK_WINDOW (window),
                                 currentFontDesc, NULL,
                                 onFontChosen, NULL);
}

/* Build the menu bar model. Mutually-exclusive groups render as radios
 * because each item carries a string target ("app.units::m" etc.) that
 * GTK compares against the action's string state. */
static GMenuModel *buildMenuModel (void) {
    GMenu *menubarModel = g_menu_new ();
    GMenu *fileMenu     = g_menu_new ();
    GMenu *unitsMenu    = g_menu_new ();
    GMenu *unitsSection = g_menu_new ();
    GMenu *gmtSection   = g_menu_new ();
    GMenu *magSection   = g_menu_new ();
    GMenu *degreesMenu  = g_menu_new ();
    GMenu *hostMenu     = g_menu_new ();
    GMenu *helpMenu     = g_menu_new ();

    g_menu_append (fileMenu, "_Font", "app.font");
    g_menu_append (fileMenu, "_Save", "app.save");
    g_menu_append (fileMenu, "_Quit", "app.quit");
    g_menu_append_submenu (menubarModel, "_File", G_MENU_MODEL (fileMenu));

    g_menu_append (unitsSection, "_Metric", "app.units::m");
    g_menu_append (unitsSection, "_US",     "app.units::u");
    g_menu_append (unitsSection, "_Knots",  "app.units::k");
    g_menu_append_section (unitsMenu, NULL, G_MENU_MODEL (unitsSection));

    g_menu_append (gmtSection, "_Gmt",   "app.gmt::g");
    g_menu_append (gmtSection, "_Local", "app.gmt::l");
    g_menu_append_section (unitsMenu, NULL, G_MENU_MODEL (gmtSection));

    g_menu_append (magSection, "_True",     "app.magnetic::t");
    g_menu_append (magSection, "Magn_etic", "app.magnetic::m");
    g_menu_append_section (unitsMenu, NULL, G_MENU_MODEL (magSection));
    g_menu_append_submenu (menubarModel, "_Units", G_MENU_MODEL (unitsMenu));

    g_menu_append (degreesMenu, "_ddd.dddddd",   "app.angle::d");
    g_menu_append (degreesMenu, "ddd _mm.mmmm",  "app.angle::m");
    g_menu_append (degreesMenu, "ddd mm _ss.ss", "app.angle::s");
    g_menu_append_submenu (menubarModel, "_Degrees", G_MENU_MODEL (degreesMenu));

    g_menu_append (hostMenu, "_Resynch", "app.resynch");
    g_menu_append (hostMenu, "_Host",    "app.host");
    g_menu_append_submenu (menubarModel, "_Host", G_MENU_MODEL (hostMenu));

    g_menu_append (helpMenu, "_About", "app.about");
    g_menu_append_submenu (menubarModel, "_Help", G_MENU_MODEL (helpMenu));

    g_object_unref (fileMenu);
    g_object_unref (unitsSection);
    g_object_unref (gmtSection);
    g_object_unref (magSection);
    g_object_unref (unitsMenu);
    g_object_unref (degreesMenu);
    g_object_unref (hostMenu);
    g_object_unref (helpMenu);

    return G_MENU_MODEL (menubarModel);
}

/* Register the application actions and their accelerators. The radio
 * groups are created stateful with their initial value from the current
 * settings, applying the old setActive* default-normalization first. */
static void setupActions (GtkApplication *application) {
    GSimpleAction *a;
    char buf[2] = { 0, 0 };
    gsize i;

    static const GActionEntry actionEntries[] = {
        { "resynch", resynchAction, NULL, NULL, NULL },
        { "host",    hostAction,    NULL, NULL, NULL },
        { "about",   aboutAction,   NULL, NULL, NULL },
        { "save",    saveAction,    NULL, NULL, NULL },
        { "quit",    quitAction,    NULL, NULL, NULL },
        { "font",    fontAction,    NULL, NULL, NULL },
    };

    static const struct {
        const char *action;
        const char *accel;
    } accels[] = {
        { "app.font",        "<Ctrl>f" },
        { "app.save",        "<Ctrl>s" },
        { "app.quit",        "<Ctrl>q" },
        { "app.units::m",    "<Ctrl>m" },
        { "app.units::u",    "<Ctrl>i" },
        { "app.units::k",    "<Ctrl>k" },
        { "app.gmt::g",      "<Ctrl>g" },
        { "app.gmt::l",      "<Ctrl>l" },
        { "app.magnetic::t", "<Ctrl>t" },
        { "app.magnetic::m", "<Ctrl>e" },
        { "app.angle::d",    "<Ctrl>d" },
        { "app.angle::m",    "<Ctrl>n" },
        { "app.angle::s",    "<Ctrl><Shift>s" },
        { "app.resynch",     "<Ctrl>r" },
        { "app.host",        "<Ctrl>h" },
        { "app.about",       "<Ctrl>a" },
    };

    g_action_map_add_action_entries (G_ACTION_MAP (application),
                                     actionEntries,
                                     G_N_ELEMENTS (actionEntries), NULL);

    if (units != METRIC && units != US && units != KNOTS) {
        units = US;
    }
    buf[0] = units;
    a = g_simple_action_new_stateful ("units", G_VARIANT_TYPE_STRING,
                                      g_variant_new_string (buf));
    g_signal_connect (a, "change-state", G_CALLBACK (unitsChangeState), NULL);
    g_action_map_add_action (G_ACTION_MAP (application), G_ACTION (a));
    g_object_unref (a);

    if (angle != DEGREES && angle != MINUTES && angle != SECONDS) {
        angle = DEGREES;
    }
    buf[0] = angle;
    a = g_simple_action_new_stateful ("angle", G_VARIANT_TYPE_STRING,
                                      g_variant_new_string (buf));
    g_signal_connect (a, "change-state", G_CALLBACK (angleChangeState), NULL);
    g_action_map_add_action (G_ACTION_MAP (application), G_ACTION (a));
    g_object_unref (a);

    buf[0] = gmt ? 'g' : 'l';
    a = g_simple_action_new_stateful ("gmt", G_VARIANT_TYPE_STRING,
                                      g_variant_new_string (buf));
    g_signal_connect (a, "change-state", G_CALLBACK (gmtChangeState), NULL);
    g_action_map_add_action (G_ACTION_MAP (application), G_ACTION (a));
    g_object_unref (a);

    buf[0] = magnetic ? 'm' : 't';
    a = g_simple_action_new_stateful ("magnetic", G_VARIANT_TYPE_STRING,
                                      g_variant_new_string (buf));
    g_signal_connect (a, "change-state", G_CALLBACK (magneticChangeState), NULL);
    g_action_map_add_action (G_ACTION_MAP (application), G_ACTION (a));
    g_object_unref (a);

    for (i = 0; i < G_N_ELEMENTS (accels); i++) {
        const char *v[] = { accels[i].accel, NULL };
        gtk_application_set_accels_for_action (application, accels[i].action, v);
    }
}

/* Our display function. Nested case statements. */
void showData (void) {
    char tmpBuff[STRINGBUFFSIZE];   /* generic temporary holding. */

    /* Some things we ignore. */
    if (gpsdata.set & POLICY_SET) {
        gpsdata.set &= ~(POLICY_SET);
        return;
    }

    /* Fill in receiver type. Detect missing gps receiver. */
    if (gpsdata.set & (DEVICE_SET)) {
        if (gpsdata.dev.activated.tv_sec < 0) {
            sendWatch ();
            gpsLost = true;

            gtk_progress_bar_set_fraction (progress, 0.0);
            gtk_progress_bar_set_text (progress, "GPS lost!");
            setColor ("no-gpsd");

            if (verbose) {
                (void) fprintf (stderr, "gps lost.\n");
                (void) snprintf(tmpBuff, sizeof(tmpBuff),
                                "driver = %s: subtype = %s%s: activated = %lld",
                                gpsdata.dev.driver, gpsdata.dev.subtype,
                                gpsdata.dev.subtype1,
                                (long long)gpsdata.dev.activated.tv_sec);
                (void) printf ("gps found.\n");
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
                (void) snprintf (titleBuff, STRINGBUFFSIZE,
                                 "%s: %s; %s %s",
                                 baseName, gpsdata.dev.driver,
                                 gpsdata.dev.subtype, gpsdata.dev.subtype1);
                gtk_window_set_title (GTK_WINDOW (window), titleBuff);
            } else {
                (void) strcpy (tmpBuff, "GPS Found!");
            }
            sendWatch ();
            gtk_progress_bar_set_text (progress, tmpBuff);
            if (verbose) {
                (void) snprintf(tmpBuff, sizeof(tmpBuff),
                                "driver = %s: subtype = %s%s: activated = %lld",
                                gpsdata.dev.driver, gpsdata.dev.subtype,
                                gpsdata.dev.subtype1,
                                (long long)gpsdata.dev.activated.tv_sec);
                (void) printf ("gps found.\n");
            }
        }

        if (verbose) {
            (void) printf ("set 0x%08x, (device) GPS Driver: %s\n",
                           (unsigned int) gpsdata.set, tmpBuff);
        }

        gpsdata.set &= ~((DEVICE_SET));
        return;
    }

    if ((gpsdata.set & DEVICELIST_SET) && verbose) {
        printf ("set 0x%08x, %d device%s found.\n",
                (int) gpsdata.set,
                gpsdata.devices.ndevices,
                gpsdata.devices.ndevices == 1 ? "" : "s");
        if (gpsdata.devices.ndevices >= 1) {
            /* N.B: I have tested this with one device, but have not
             * tested it with multiple devices. */
            int i;

            for (i = 0 ; i < gpsdata.devices.ndevices; i++ ) {
                (void) printf("Device no. %i: driver = %s: subtype (if any) = %s%s\n",
                              i,
                              gpsdata.devices.list[0].driver,
                              gpsdata.devices.list[0].subtype,
                              gpsdata.devices.list[0].subtype1);
            }
        } else {
            (void) printf ("set 0x%08x, no devices reported.\n",
                           (unsigned int) gpsdata.set);
        }
        gpsdata.set &= ~(DEVICELIST_SET);
        return;
    }

    if (gpsdata.set & TIME_SET) {
        formatTime (gpsdata.fix.time);
        gpsdata.set &= ~(TIME_SET);
    }

    if (gpsdata.set & VERSION_SET && (verbose != false)) {
        (void) printf ("set 0x%08x, GPSD version: %s Rev: %s, Protocol version %d.%d\n",
                       (unsigned int) gpsdata.set,
                       gpsdata.version.release,
                       gpsdata.version.rev,
                       gpsdata.version.proto_major,
                       gpsdata.version.proto_minor);
        gpsdata.set &= ~(VERSION_SET);
        return;
    }

    if ((gpsdata.set & SPEED_SET)
            && isfinite (gpsdata.fix.speed)) {
        formatSpeed (gpsdata.fix.speed);
        gpsdata.set &= ~(SPEED_SET);
    }

    if (magnetic == false) {
        if ((gpsdata.set & TRACK_SET)
                && isfinite (gpsdata.fix.track)) {
            formatTrack (gpsdata.fix.track);
            gpsdata.set &= ~(TRACK_SET);
        }
    } else {
        if ((gpsdata.set & MAGNETIC_TRACK_SET)
                && isfinite (gpsdata.fix.magnetic_track)) {
            formatTrack (gpsdata.fix.magnetic_track);
            gpsdata.set &= ~(MAGNETIC_TRACK_SET);
        }
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
        gpsdata.set &= ~(SATELLITE_SET);
        return;
    } /* SATELLITE_SET */

    if (verbose) {
        printf ("Mask is %s.\n",
                gps_maskdump (gpsdata.set));
    }

    if (gpsdata.set & STATUS_SET) {
        int status = (gpsdata.fix.status);
        int mode = gpsdata.fix.mode;

        /* Condition the mode. */
        if (0 > mode ||
                SIZEMODESTRINGS <= mode) {
            mode = 0;
        }

        if (gpsdata.set & MODE_SET) {

            switch (mode) {
            case MODE_NOT_SEEN:
                setColor ("no-fix");
                (void) strcpy (fixBuff, "Fix not yet seen");
                break;

            case MODE_NO_FIX:
                setColor ("no-fix");
                (void) strcpy (fixBuff, "No fix yet");
                break;

            case MODE_2D:
                setColor ("gps-2d");
                if ((gpsdata.set & LATLON_SET)
                        && isfinite(gpsdata.fix.latitude)
                        && isfinite(gpsdata.fix.longitude)) {
                    formatLat (gpsdata.fix.latitude);
                    formatLong (gpsdata.fix.longitude);

                    if (verbose) {
                        (void) snprintf (fixBuff, STRINGBUFFSIZE,
                                         "la %f, lo %f",
                                         gpsdata.fix.latitude,
                                         gpsdata.fix.longitude);
                    }
                }
                break;

            case MODE_3D:
                setColor ("gps-3d");
                if ((gpsdata.set & (LATLON_SET))
                        && isfinite(gpsdata.fix.latitude)
                        && isfinite(gpsdata.fix.longitude)) {
                    formatLat (gpsdata.fix.latitude);
                    formatLong (gpsdata.fix.longitude);
                }

                /* Only if we have a fix are the details of it
                   useful. Altitude is only meaningful on a 3D
                   fix. */
                if ((gpsdata.set & (ALTITUDE_SET))
                        && isfinite(gpsdata.fix.latitude)
                        && isfinite(gpsdata.fix.longitude)
                        && isfinite(gpsdata.fix.altitude)) {
                    formatAltitude (gpsdata.fix.altitude);
                    if (verbose) {

                        (void) snprintf (fixBuff, STRINGBUFFSIZE,
                                         "la %f, lo %f, alt %f",
                                         gpsdata.fix.latitude,
                                         gpsdata.fix.longitude,
                                         gpsdata.fix.altitude);
                    }
                }

                break;

            default:
                (void) fprintf (stderr, "%s: Catastrophic error: Invalid mode %d, %s. Status: %d.\n",
                                baseName,
                                mode, modeString[mode],
                                status);
                setColor ("no-fix");
                return;
            } /* fix.mode */
        } /* MODE_SET */

        if (verbose) {
            (void) printf ("set 0x%08x, %s, mode: |%s|, status |%s|, %s.\n",
                           (unsigned int) gpsdata.set,
                           timeString,
                           modeString[mode],
                           getStatusString (status),
                           fixBuff);
        }
        gpsdata.set &= ~(STATUS_SET);
    } /* STATUS_SET */
}

/* Build a pair of display widgets, a label followed by an entry
 * box. N.B.: This routine does not initialize the strings for the
 * widgets. Do that in the calling function with initStrings. */
static void buildPair (gint index, gchar *labelText, GtkWidget *grid,
                       gint left, gint top) {
    GtkWidget *child;
    left <<= 1;                 /* Convert pair co-ordinates to column
                                 * co-ordinates by multiplying by 2. */

    /* A right aligned label. */
    child = gtk_label_new( labelText );
    gtk_widget_set_halign (child, GTK_ALIGN_END);
    gtk_widget_set_valign (child, GTK_ALIGN_CENTER);
    gtk_grid_attach (GTK_GRID (grid), child, left, top, 1, 1);

    /* And now the text box to show the value. */
    child = gtk_entry_new ();
    gtk_editable_set_editable(GTK_EDITABLE (child), true);

    /* Ask for a slightly longer time widget. */
    gtk_editable_set_width_chars (GTK_EDITABLE (child), left ? 22 : 17);
    gtk_widget_set_hexpand (child, TRUE);
    gtk_widget_set_halign (child, GTK_ALIGN_FILL);
    gtk_grid_attach (GTK_GRID (grid), child, left + 1, top, 1, 1);

    /* Remember it so we can update its text and color later. */
    preserve (GTK_ENTRY (child), index);
}

/* Return true so we get called forever. */
gint gpsPoll (gpointer data) {
    if (haveConnection == false ) {
        resynch ();
        return (true);
    }

    /*  timeout in microseconds. .02 seconds. */
    /* if (gps_waiting (&gpsdata, 20000) == true) { */
    /* Trial: 5 seconds. From https://gpsd.io/gpsd-client-example-code.html */
    if (gps_waiting (&gpsdata, 5000000) == true) {
        int ret;

        ret = gps_read (&gpsdata, NULL, 0);
        if (ret == -1) {
            if (verbose) {
                fprintf (stderr, "Gnome-gps: bad read from gps_read(), %d. ", ret);
                perror (NULL);
            }
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

    sandbox [0] = magnetic ? 't' : 'f';
    g_key_file_set_value (keyFile, baseName, "magnetic", sandbox);

    /* Save the display font. Prefer the user's chosen font; if none was
     * chosen, fall back to the entry's effective font so we always write
     * something, as the GTK2 version did. */
    {
        PangoFontDescription *desc = currentFontDesc;
        gboolean ownDesc = FALSE;

        if (desc == NULL) {
            desc = pango_font_description_copy (
                       pango_context_get_font_description (
                           gtk_widget_get_pango_context (GTK_WIDGET (entries[TIME]))));
            ownDesc = TRUE;
        }
        fontName = pango_font_description_to_string (desc);
        g_key_file_set_value (keyFile, baseName, "font", fontName);
        g_free ((gpointer) fontName);
        if (ownDesc) {
            pango_font_description_free (desc);
        }
    }

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

/* The menu radios are initialized from these settings by the GActions
 * created in setupActions (); the old setActive* helpers are gone. */

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

/* Build the user interface and wire up gpsd. Called once when the
 * GtkApplication is activated. */
static void on_activate (GtkApplication *application, gpointer data) {
    GtkCssProvider *provider;
    GMenuModel *menuModel;

    /* Main window. */
    window = gtk_application_window_new (application);
    (void) snprintf (titleBuff, STRINGBUFFSIZE,
                     "%s: a simple GTK+ GPS monitor", baseName);
    gtk_window_set_title (GTK_WINDOW (window), titleBuff);
    gtk_window_set_icon_name (GTK_WINDOW (window), "gnome-gps");
    g_signal_connect (window, "close-request",
                      G_CALLBACK (on_close_request), NULL);

    /* Fix-state background colors, applied via a CSS provider on the
     * default display and selected by a class on the root container. */
    provider = gtk_css_provider_new ();
    gtk_css_provider_load_from_data (provider,
                                     ".gps-3d, .gps-3d entry, .gps-3d entry > text,"
                                     " .gps-3d progressbar > trough, .gps-3d progressbar > trough > progress,"
                                     " .gps-3d progressbar > text, .gps-3d menubar, .gps-3d menubar > item"
                                     " { background: #00ff00; }"
                                     ".gps-2d, .gps-2d entry, .gps-2d entry > text,"
                                     " .gps-2d progressbar > trough, .gps-2d progressbar > trough > progress,"
                                     " .gps-2d progressbar > text, .gps-2d menubar, .gps-2d menubar > item"
                                     " { background: #ffff00; }"
                                     ".no-fix, .no-fix entry, .no-fix entry > text,"
                                     " .no-fix progressbar > trough, .no-fix progressbar > trough > progress,"
                                     " .no-fix progressbar > text, .no-fix menubar, .no-fix menubar > item"
                                     " { background: #ff0000; }"
                                     ".no-gpsd, .no-gpsd entry, .no-gpsd entry > text,"
                                     " .no-gpsd progressbar > trough, .no-gpsd progressbar > trough > progress,"
                                     " .no-gpsd progressbar > text, .no-gpsd menubar, .no-gpsd menubar > item"
                                     " { background: #808080; }"
                                     " .gnome-gps-ui entry { box-shadow: none; }",
                                     -1);
    gtk_style_context_add_provider_for_display (gdk_display_get_default (),
            GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_USER);
    colorProvider = provider;

    /* A second provider, for the user-selected display font. */
    fontProvider = gtk_css_provider_new ();
    gtk_style_context_add_provider_for_display (gdk_display_get_default (),
            GTK_STYLE_PROVIDER (fontProvider), GTK_STYLE_PROVIDER_PRIORITY_USER);

    /* Vertical box: menu bar, grid of readings, then progress bar. */
    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 1);
    gtk_widget_set_margin_start  (vbox, 1);
    gtk_widget_set_margin_end    (vbox, 1);
    gtk_widget_set_margin_top    (vbox, 1);
    gtk_widget_set_margin_bottom (vbox, 1);
    gtk_widget_add_css_class (vbox, "gnome-gps-ui");
    gtk_window_set_child (GTK_WINDOW (window), vbox);

    /* Menu bar. The bar takes its own reference to the model, so drop
     * the one buildMenuModel () returned. */
    setupActions (application);
    menuModel = buildMenuModel ();
    menubar = gtk_popover_menu_bar_new_from_model (menuModel);
    g_object_unref (menuModel);
    gtk_box_append (GTK_BOX (vbox), menubar);

    /* Grid of label/value pairs. */
    table = gtk_grid_new ();
    gtk_grid_set_row_spacing (GTK_GRID (table), YPADDING);
    gtk_grid_set_column_spacing (GTK_GRID (table), XPADDING);
    gtk_widget_set_hexpand (table, TRUE);
    gtk_widget_set_vexpand (table, TRUE);
    gtk_box_append (GTK_BOX (vbox), table);

    buildPair (LAT,   "Lat",   table, 0, 0);
    buildPair (LONG,  "Long",  table, 1, 0);
    buildPair (SPEED, "Speed", table, 0, 1);
    buildPair (TRACK, "Track", table, 1, 1);
    buildPair (ALT,   "Alt",   table, 0, 2);
    buildPair (TIME,  "Time",  table, 1, 2);

    initStrings ();

    /* The satellite progress bar. GTK4 hides the text unless asked. */
    progress = (GtkProgressBar *) gtk_progress_bar_new ();
    gtk_progress_bar_set_show_text (progress, TRUE);
    gtk_progress_bar_set_fraction (progress, 0.0);
    gtk_progress_bar_set_text (progress, "We've seen no data yet.");
    gtk_box_append (GTK_BOX (vbox), GTK_WIDGET (progress));

    /* Apply the saved font, if any. */
    if (initialFont != NULL && strlen (initialFont) > 0) {
        currentFontDesc = pango_font_description_from_string (initialFont);
        applyFontCss ();
    }

    gtk_window_present (GTK_WINDOW (window));

    /* Connect to gpsd and start polling. */
    gpsdata.set = 0;
    resynch ();
    tag = g_timeout_add ((guint32) 100, gpsPoll, NULL);
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
    (void) printf ("Version %s, compiled for libgps API version %d.%d.\n",
                   versionString,
                   GPSD_API_MAJOR_VERSION, GPSD_API_MINOR_VERSION);

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

            conf->magnetic = NULL;
            conf->magnetic = g_key_file_get_string ( keyFile, baseName,
                             "magnetic", NULL);
            if (conf->magnetic != NULL && strlen (conf->units) > 0) {
                magnetic = (conf->magnetic[0] == 't') ? true : false;
            }
        }
    }

    /* for option processing. */
    char *optstring = "d:eghlkmp:tuv";
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

        case 'g':
            gmt = true;
            break;

        case 'l':
            gmt = false;
            break;

        case 'e':
            magnetic = true;
            break;

        case 't':
            magnetic = false;
            break;

        case 'p':
            (void) strncpy (hostPort, optarg, STRINGBUFFSIZE-1);
            break;

        case 'v':
            verbose = true;
            break;

        case 'h':               /* Falls through to print out the usage message. */

        default:                /* '?' */
            (void) fprintf(stderr,
                           "Usage: %s [-d d] [-e] [-g] [-h] [-l] [-m] [-k] [-p port] [-t] [-u] [-v] [host]\n",
                           baseName);
            return(-2);
        }
    }

    if (optind != argc) {
        /* we have a host name.... */
        (void) strncpy (hostName, argv[optind], STRINGBUFFSIZE-1);
    }

#ifdef DEBUG
    if (verbose) {
        showStatusStrings ();
    }
#endif

    /* Hand the configured font off to the activate handler. */
    initialFont = conf->font;

    /* GtkApplication owns the main loop now. We parse our own options
     * above, so don't pass argv on to GApplication. */
    {
        GtkApplication *application;

        application = gtk_application_new ("com.charlescurley.gnome-gps",
                                           G_APPLICATION_NON_UNIQUE);
        g_signal_connect (application, "activate",
                          G_CALLBACK (on_activate), NULL);
        ret = g_application_run (G_APPLICATION (application), 0, NULL);
        g_object_unref (application);
    }

    if (baseName != NULL) {
        g_free((gpointer) baseName);
    }

    if (keyFile != NULL) {
        g_free(keyFile);
    }

    return (ret);
}
