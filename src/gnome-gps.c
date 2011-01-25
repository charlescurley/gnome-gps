char *versionString = "Time-stamp: <2011-01-16 15:57:50 charles gnome-gps.c>";

/* A GTK program for showing the latest gps data using libgps. */

/* Prerequisites. Any fairly recent version should do.

   aptitude install gcc libgtk2.0-dev

   Compile:

   gcc -Wall gnome-gps.c -o gnome-gps $(pkg-config --cflags --libs gtk+-2.0 libgps)

   add: -ggdb for debug (gdb) output.

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

/* Our icon in raw format. Swiped from the gpsd distribution, which is
 * distributed under the BSD license. I auto-cropped it to make it
 * square, then scaled it to 32x32, a compromise between file size and
 * clarity. I then generated the C source with: */

/* gdk-pixbuf-csource gpsd-logo-small.png  > icon.image.c */

/* GdkPixbuf RGBA C-Source image dump 1-byte-run-length-encoded */

#ifdef __SUNPRO_C
#pragma align 4 (iconPixBuf)
#endif
#ifdef __GNUC__
const guint8 iconPixBuf[] __attribute__ ((__aligned__ (4))) =
#else
const guint8 iconPixBuf[] =
#endif
{ ""
        /* Pixbuf magic (0x47646b50) */
        "GdkP"
        /* length: header (24) + pixel_data (3048) */
        "\0\0\14\0"
        /* pixdata_type (0x2010002) */
        "\2\1\0\2"
        /* rowstride (128) */
        "\0\0\0\200"
        /* width (32) */
        "\0\0\0\40"
        /* height (32) */
        "\0\0\0\40"
        /* pixel_data: */
        "\215\0\0\0\0\3\237ofT\225xsd\331F$\15\206\0\0\0\0\1\354J*\0\220\0\0\0"
        "\0\1\355G'\0\203\0\0\0\0\12\351]B\40\314\232\220_\243las\216~z\350\235"
        "\216\261\330\237\207\215\270\230rl\204\317\203t\77\305\237\230|\333u"
        "a1\203\0\0\0\0\1\352P2\0\214\0\0\0\0\26\276\263\260\0\0\0\0\0\377\0\0"
        "\4\301\242\234Y\272\254\252\260\273\255\252\366\271\257\254\344\272\255"
        "\253\266\377aB\14\0\0\0\0\254\215\204\247\230zs\200\215\177|\311\310"
        "\214\200:\271\254\251\246\271\257\255\324\273\253\250\366\271\255\253"
        "\303\277\246\241h\353`C\16\0\0\0\0e\377\377\0\210\0\0\0\0\10\322\211"
        "z\0\0\0\0\0\355N2\1\315\217\203L\270\257\255\365\265\264\264\366\304"
        "\241\231\213\3747\21\20\205\0\0\0\0\1\377\377\203\0\204\0\0\0\0\6\377"
        "\0\0\0\313\223\211d\267\262\261\353\267\262\261\376\310\230\216j\351"
        "R4\2\202\0\0\0\0\1\335G)\0\206\0\0\0\0\5\377\25\0\10\273\254\251\301"
        "\266\263\262\377\301\241\233o\342qY\16\202\0\0\0\0\1\312\244\235\0\202"
        "\0\0\0\0\7;\31\24z\26\13\12\270\24\12\12\275'\22\17\232\226\77-.\0\0"
        "\0\0\311I1\0\202\0\0\0\0\6\377\0\0\2\304\235\226O\272\256\254\362\321"
        "\215\200\\\206\\Tg\310^H\6\206\0\0\0\0\4\335r]\10\271\256\254\327\265"
        "\262\261\362\330\177m)\202\0\0\0\0\15\320F+\0\0\0\0\0\337G*\0\377sI\4"
        "$\21\16\331\0\0\2\377\1\1\3\377\1\1\2\377\11\12\12\377\15\17\17\377\77"
        "\33\25o\0\0\0\0-%$\0\203\0\0\0\0\4\227@.0\202|{\374\206\211\212\377\220"
        "^Ti\206\0\0\0\0\5\300\245\237\262\264\265\266\377\322\210y;\0\0\0\0\214"
        "F85\204\0\0\0\0\3""3\25\22W\1\1\3\377\1\1\2\376\202\0\0\1\377\4\16\16"
        "\16\376$$$\377\2\1\2\377\266J5&\204\0\0\0\0\4\217f^\24\210\204\203\364"
        "\207\207\207\377\204{z\364\204\0\0\0\0\11\355H(\0\320\215\200,\264\266"
        "\266\377\276\247\242\264\0\0\0\0\237\0\0\14\206\177}\362\202xv\354\207"
        "[RJ\202\0\0\0\0\4)\22\16\215\0\0\2\376\1\1\3\377\2\2\3\377\203\0\0\0"
        "\377\2\0\0\2\3777\30\23\223\204\0\0\0\0\5\276\21\0\2\206{y\307\210\210"
        "\210\377\202\203\204\377\214[QD\204\0\0\0\0\23\277\245\240c\264\265\265"
        "\377\302\237\230n\0\0\0\0\202tq\316\206\207\207\377\210\210\210\377\202"
        "\202\203\377\233-\26*\0\0\0\0\"\16\13\234)))\377!!\"\377\0\0\1\377**"
        "+\377aaa\377\27\27\30\377\0\0\1\377&\21\16\275\205\0\0\0\0\3\210WMU\206"
        "\203\201\373\256\242\240\360\202\0\0\0\0\1\344G'\0\202\0\0\0\0\10\276"
        "\246\242g\264\265\265\377\302\247\241\202\204a[`\211\211\211\377\207"
        "\207\207\376\212\212\211\377\206ke\201\202\0\0\0\0\11\77($\226\200\200"
        "\200\377\215\216\216\377\11\12\15\377\253\254\254\377jjj\377ttu\377\0"
        "\0\0\377\27\13\14\322\205\0\0\0\0\4\276\234\226k\204[S\213\203\202\313"
        "\373\203s\250\321\203\0\0\0\0\25\355I(\0\307\232\2208\265\262\262\364"
        "\300\251\244\202\207_Wj\204\204\203\376\211\212\212\377\201zy\344\245"
        "\0\0\7\271D-\0\0\0\0\0gPK\216JJJ\376[Q@\377YC\13\377~v`\377''%\377\260"
        "\260\261\377\0\0\0\377\14\6\10\340\377\335\202\2\202\0\0\0\0\6\355F%"
        "\0\240\0\0\2\316\305\303\317\210\203\200\361\201y\254\351\243\230\310"
        "\357\204\0\0\0\0\7\377\0\0\0\303\230\217\23\245\177\214\200\246\234\261"
        "\332\311\310\307\377wZU\306\245G4\33\203\0\0\0\0\12(\17\16\204aM2\377"
        "\260\207\3\377\355\311\16\377\363\340\35\377\334\274\34\377\\O\34\377"
        "\0\0\2\376\6\3\5\352\231A/\13\202\0\0\0\0\10n\377\377\0\0\0\0\0\277\231"
        "\221\212\220\210\206\314\234NCD\267\264\265\373\203oj\262\274(\13\7\203"
        "\0\0\0\0\7\234[k\17\214\207\307\376~~\327\377\212PW_a\0\0\22\0\0\0\0"
        "\0v\227\0\202\0\0\0\0\12""0\24\16w^7\1\377\345\270\11\377\361\325\23"
        "\377\347\331\"\377\311\236\10\377`E\7\377\12\13\20\377\7\5\6\372N\40"
        "\26+\202\0\0\0\0`\312\224\212/\273\252\247\317\274\247\244\345\0\0\0"
        "\0\210ke\266\201\202\202\377\207\207\207\376\217jb3\315F,\0\0\0\0\0\277"
        "I1\22\245gc\16\221\215\301\370\211~\243\344ULI\352\244\247\250\377\223"
        "PCN\0\0\0\0\320\214\177\0\0\0\0\0#\12\6vg`Z\377\255\202\15\377\331\263"
        "\13\377\274\205\1\377veU\377fff\377\30\30\30\377QQR\377+\23\16\276\377"
        "\334\274\13\301\244\236\257\264\265\265\377\265\264\264\377\302\237\231"
        "\231\0\0\0\0\220L\77I\206\210\210\377\210\210\211\377\211ni\220\0\377"
        "\377\0\0\0\0\0\202ur\326\202xw\356\253\243\241\361w\77""5D\327\330\330"
        "\377\253\234\231\330\300\237\230\265\267\260\257\365\270\256\254\300"
        "\303\256\252\210A;;\360\272\272\272\376voi\377ucO\377\215\206\205\377"
        "\346\347\350\377\351\351\351\377VVV\377ccc\376\212\212\213\377\270\265"
        "\265\377\263\266\266\377\273\252\247\316\302\237\232F\0\0\0\0\253\245"
        "\244\0\0\0\0\0\207}{\343\210\211\212\377\204ur\351\0\0\0\0\203b\\\226"
        "\207\211\212\377\213\213\213\377\200}}\377\220@0\77\222`W]\377\0\0\5"
        "\311\225\213X\270\260\257\332\266\263\262\361\262\262\263\377\271\271"
        "\271\377\260\260\260\377\221\221\221\377\214\214\214\377\263\263\263"
        "\377\257\257\257\377\260\260\260\377\266\266\266\377\260\260\260\377"
        "\244\244\245\377\216\212\212\373\314\177pF\202\0\0\0\0\1\347X<\0\202"
        "\0\0\0\0\10\204jc|\214f^y\377\0\0\0\216e]7\202\201\201\374\210\210\210"
        "\376\211\211\212\377\204lg\254\205\0\0\0\0\16X3+\210\27\27\30\377\321"
        "\321\321\377\334\334\334\377\275\275\275\377\312\312\312\377\320\320"
        "\320\377\330\330\330\377\343\343\343\377\276\276\276\377\12\12\13\377"
        "\11\11\12\377\0\0\2\377$\20\16\305\204\0\0\0\0\1\231J;\0\203\0\0\0\0"
        "\6\205`Yh\206\203\202\376\212\212\213\377\202}|\365\217PC\27ZNL\0\203"
        "\0\0\0\0\20\2209(&\5\3\4\377\15\15\16\377\255\255\255\377\367\367\367"
        "\377\312\312\312\377\352\352\352\377\377\377\377\377\342\342\342\377"
        "\261\261\261\377\215\215\215\377JJK\377\21\21\22\377\4\4\4\376\0\0\2"
        "\377;\33\26^\210\0\0\0\0\5\255='\35~us\360\216G8G\0\0\0\0\350G'\0\202"
        "\0\0\0\0\4\377\240_\0\37\13\7\236\37\37\40\377\224\224\224\376\202\377"
        "\377\377\377\1\376\376\376\377\203\377\377\377\377\11\376\376\376\377"
        "\370\370\370\377\225\225\225\377\17\17\17\377\15\15\16\377\2\2\3\377"
        "\32\14\13\330\0\0\0\0\274B*\0\205\0\0\0\0\5\245D1\0\0\0\0\0\324\0\0\7"
        "\0\0\0\0\201LB\0\202\0\0\0\0\5y3&\0\0\0\0\0#\33\31\342\37\37\37\376\367"
        "\367\367\377\202\376\376\376\377\2\377\377\377\377\376\376\376\377\204"
        "\377\377\377\377\6\376\376\376\377KKK\377'''\377\6\6\7\377\2\2\3\377"
        "\224;*/\216\0\0\0\0\5k+\37L\15\15\16\377xxx\377\377\377\377\377\376\376"
        "\376\377\203\377\377\377\377\202\376\376\376\377\10\376\377\376\377\376"
        "\376\376\377\377\377\377\377yyy\377\0\0\0\377\26\26\27\377\0\0\1\377"
        "8\30\23\223\215\0\0\0\0\6\377\252q\6\15\7\7\363\1\1\1\377\342\342\342"
        "\377\376\376\376\377\376\376\375\377\203\376\376\376\377\12\375\376\376"
        "\377\377\376\376\377\375\376\375\377\376\375\375\377\377\377\377\377"
        "\235\236\235\377\0\0\0\377\25\25\25\377\0\0\1\377\33\15\13\320\215\0"
        "\0\0\0\4Q#\30/\0\0\0\377\17\17\20\376\374\374\374\377\202\376\376\376"
        "\377\16\377\377\376\377\377\377\377\377\376\377\376\377\376\376\376\377"
        "\376\377\377\377\375\376\376\377\376\376\376\377\377\377\377\377\230"
        "\231\230\377\13\13\13\377\5\5\5\377\0\0\0\377\12\5\5\346\377\377\352"
        "\1\213\0\0\0\0\6\357J*\0z96$RA\4\377\"\32\3\377\335\335\334\377\376\376"
        "\375\377\203\377\377\377\377\202\375\375\374\377\11\376\376\376\377\375"
        "\375\375\377\376\375\375\377\377\377\376\377mmk\377\21\21\21\377\6\6"
        "\7\37789:\377,\20\14\253\211\0\0\0\0\12\350Y7\0\0\0\0\0\376ay\14\262"
        "iG2\244n\35\214\334\254\0\377\325\252\1\377;2\30\377\305\305\306\377"
        "\377\377\377\377\202\376\376\376\377\205\377\377\377\377\2\244\200@\377"
        "\212j\5\377\202\0\0\2\377\3=4\25\377\350\260\40\265\377\20\323\1\212"
        "\0\0\0\0\10\261\210\27\375\312\236\0\377\330\250\0\377\344\262\0\376"
        "\345\265\0\377\262\224\3\377\0\0\0\377\230\230\230\377\206\377\377\377"
        "\377\12\311\313\316\377}U\30\377\311\232\3\377#\26\5\377*\34\5\377\274"
        "\222\4\376\343\261\10\357\350\203g\15\323\263O\0\355F'\0\207\0\0\0\0"
        "\2\377\0\377\3\267\216\17\377\202\344\263\0\377\6\343\263\1\377\343\262"
        "\1\377\343\265\1\377VB\5\377\0\0\0\377\243\243\243\377\205\377\377\377"
        "\377\10\271\273\277\377|U\34\377\334\253\0\377\326\235\0\377\335\246"
        "\0\377\344\263\0\377\342\261\2\376\332\2275m\210\0\0\0\0\5\314\201T\0"
        "\0\0\0\0\262~\23\350\343\262\0\377\344\263\0\377\202\344\263\1\377\4"
        "\344\263\0\377\332\261\2\3771)\33\377\301\301\301\377\202\377\377\377"
        "\377\1\376\376\376\377\202\377\377\377\377\12\226\230\234\377T2\0\377"
        "\333\250\0\377\344\264\1\377\344\263\1\377\344\262\1\377\344\263\0\376"
        "\336\257\5\377\327\230$\233\322~=\2\207\0\0\0\0\3\377\26\304\6\262\210"
        "\15\377\343\262\0\377\202\344\263\1\377\5\344\262\1\377\343\262\0\377"
        "\346\267\0\377\260\216\36\377\374\374\374\377\203\377\377\377\377\14"
        "\375\375\375\377ddd\377\0\0\2\377kF\0\377\334\252\0\377\344\263\1\377"
        "\343\262\1\377\344\263\1\377\343\262\0\376\341\257\0\377\324\232\33\312"
        "\333zN\2\207\0\0\0\0\30\266x:Z\310\236\0\377\343\262\0\377\346\265\1"
        "\376\345\263\0\377\344\263\0\377\344\263\1\377\344\262\0\377\316\233"
        "\0\3775'\22\377VXZ\377`ab\377CEE\377\10\12\13\377\0\0\0\377\10\6\2\377"
        "rL\1\376\327\244\1\377\344\263\1\377\344\263\1\376\322\243\0\377\311"
        "\221\25\343\322\222*n\377*\313\5\210\0\0\0\0\25\377)l\3\271|5\200\263"
        "\204\35\320\254\203\7\363\310\230\2\372\326\245\1\377\340\255\1\377\340"
        "\254\1\376\306\206\1\3770\34\5\366'\20\17\2736\30\23\240:\26\20\2407"
        "\30\23\240/\25\20\265'\25\16\331dB\0\377\300\217\0\376\334\253\0\377"
        "\304\226\1\376\251p\"\227\202\0\0\0\0\1\356M.\0\207\0\0\0\0\1\341iC\0"
        "\203\0\0\0\0\7\266<N\14|D\31+\206J\26r\204M\15\335\216W\2\377|E\15\346"
        "\236H/\36\206\0\0\0\0\7X3\17\323]@\1\377sN\11\367\221T\33Y\0\0\0\0!\340"
        "Q\0\355N.\0\205\0\0\0\0"};



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

/* libgps callback. */
void showData (struct gps_data_t *gpsdata, char *message, size_t len);

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
struct gps_data_t our_gps_data;

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
    (void) gps_stream(&our_gps_data, WATCH_ENABLE | WATCH_JSON, NULL);
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
        (void) gps_stream(&our_gps_data, WATCH_DISABLE, NULL);
        gps_close (&our_gps_data);
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
    (void) strcat (latString, latitude < 0 ? "S" : latitude > 0 ? "N" : "" );
    gtk_entry_set_text(entries[LAT], latString );
}

void formatLong (double longitude) {
    double longAbs = fabs (longitude);
    formatAngle (longAbs, longString);
    (void) strcat (longString, longitude < 0 ? "W" : longitude > 0 ? "E" : "" );
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
        (void) gps_stream(&our_gps_data, WATCH_DISABLE, NULL);
        gps_close (&our_gps_data);
        haveConnection = false;
    }

    gtk_progress_bar_set_fraction (progress, 0.0);
    ret = gps_open(hostName, hostPort, &our_gps_data);
    if (ret != 0) {
        if (verbose) {
            if (errno < 1) {
                (void) fprintf (stderr, "%s: gpsd error: %s\n",
                                baseName, gps_errstr(errno));
            } else {
                perror (baseName);
            }
        }
        gtk_progress_bar_set_text (progress, "No gpsd connection!");
        haveConnection = false;
    } else {
        /* If we got here, we're good to go. */
        gtk_progress_bar_set_text (progress, "Ahhh, a gpsd connection!");
        gps_set_raw_hook (&our_gps_data, showData);

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
                           "Libgps API version 5.0 or greater.\n"
                           "\"Save\" to save settings such as font and units.",
                           "copyright", "Copyright © 2009-2010 Charles Curley.",
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

/* The libgps callback function. */
void showData (struct gps_data_t *gpsdata,
               char *message,   /* The string(s) from GPSd from which
                                   libgps reconstructs the data we
                                   use. */
               size_t len) {    /* I think this is the length of
                                 * message. */
    char tmpBuff[STRINGBUFFSIZE];   /* generic temporary holding. */
    char *fixStatus = "DGPS ";  /* Default, if we even use it. */

    /* Some things we ignore. */
    if (gpsdata->set & POLICY_SET) {
        gpsdata->set &= ~(POLICY_SET);
        return;
    }

    /* Fill in receiver type. Detect missing gps receiver. */
    if (gpsdata->set & (DEVICE_SET)) {
        if (gpsdata->dev.activated < 1.0) {
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
            if (strlen (gpsdata->dev.driver)) {
                (void) snprintf(tmpBuff, sizeof(tmpBuff), "%s GPS Found!", gpsdata->dev.driver);
                (void) snprintf (titleBuff, STRINGBUFFSIZE,
                                 "%s: %s; %s",
                                 baseName, gpsdata->dev.driver, gpsdata->dev.subtype);
                gtk_window_set_title (GTK_WINDOW (window), titleBuff);
            } else {
                (void) strcpy (tmpBuff, "GPS Found!");
            }
            sendWatch ();
            gtk_progress_bar_set_text (progress, tmpBuff);
            if (verbose) {
                (void) snprintf(tmpBuff, sizeof(tmpBuff),
                                "driver = %s: subtype = %s: activated = %f",
                                gpsdata->dev.driver, gpsdata->dev.subtype,
                                gpsdata->dev.activated);
                (void) printf ("gps found.\n");
            }
        }

        if (verbose) {
            (void) printf ("set 0x%08x, (device) GPS Driver: %s\n",
                           gpsdata->set, tmpBuff);
        }

        return;
    }

    if ((gpsdata->set & DEVICELIST_SET) && verbose) {
        if (gpsdata->devices.ndevices == 1) {
            if (gpsdata->devices.list[0].activated < 1.0) {
                (void) strcpy (tmpBuff, "driver = nil: subtype = nil: activated = 0.0");
            } else {
                (void) snprintf(tmpBuff, sizeof(tmpBuff), "driver = %s: subtype = %s: activated = %f",
                                gpsdata->devices.list[0].driver,
                                gpsdata->devices.list[0].subtype,
                                gpsdata->devices.list[0].activated);
            }
        } else {
            /* We have multiple devices, which I've never seen, so
             * punt. */
            (void) snprintf(tmpBuff, sizeof(tmpBuff), "%d devices",
                            gpsdata->devices.ndevices);
        }
        (void) printf ("set 0x%08x, (Devices) GPS Driver: %s\n",
                       gpsdata->set, tmpBuff);

        return;
    }

    if (gpsdata->set & TIME_SET) {
        formatTime (gpsdata->fix.time);
    }

    if (gpsdata->set & VERSION_SET && (verbose != false)) {
        (void) printf ("set 0x%08x, GPSD version: %s Rev: %s, Protocol %d.%d\n",
                       gpsdata->set,
                       gpsdata->version.release,
                       gpsdata->version.rev,
                       gpsdata->version.proto_major,
                       gpsdata->version.proto_minor);
    }

    if (gpsdata->set & SPEED_SET) {
        formatSpeed (gpsdata->fix.speed);
    }

    if (gpsdata->set & TRACK_SET) {
        formatTrack (gpsdata->fix.track);
    }

    /* A nice and unusual use of a progress bar, if I say so
     * myself. */
    if (gpsdata->set & SATELLITE_SET) {
        gchar banner[STRINGBUFFSIZE];

        /* Guard against division of or by 0. */
        if (gpsdata->satellites_visible > 0 && gpsdata->satellites_used > 0) {
            gtk_progress_bar_set_fraction (progress,
                                           (gdouble) gpsdata->satellites_used/
                                           (gdouble) gpsdata->satellites_visible);

            (void) snprintf (banner, STRINGBUFFSIZE,
                             "%2d satellites visible, %2d used in the fix.",
                             gpsdata->satellites_visible,
                             gpsdata->satellites_used);
        } else {
            gtk_progress_bar_set_fraction (progress, 0.0);
            if (gpsdata->satellites_visible > 0) {
                (void) snprintf (banner, STRINGBUFFSIZE,
                                 "%2d satellites visible, no fix.",
                                 gpsdata->satellites_visible);
                /* Something is fishy. If we've had satellites
                 * visible, and then lose them,
                 * gpsdata->satellites_visible still reports 1, but
                 * not 0. I think. This is to re-set it just in
                 * case. */
                /* gpsdata->satellites_visible = 0; */
            } else {
                (void) snprintf (banner, STRINGBUFFSIZE,
                                 "No satellites visible, no fix.");
            }
        }
        gtk_progress_bar_set_text (progress, banner);

        if (verbose) {
            (void) printf ("set 0x%08x, %s\n", gpsdata->set,
                           banner);
        }
    }

    if (gpsdata->set & STATUS_SET) {
        switch (gpsdata->status) {
        case STATUS_NO_FIX:
            (void) strcpy (fixBuff, "No fix");
            setColor (&NoFixColor);
            break;

        case STATUS_FIX:
            (void) strcpy (fixBuff, "No fix");
            fixStatus = "";

        case STATUS_DGPS_FIX:

            if (gpsdata->set & MODE_SET) {
                switch (gpsdata->fix.mode) {
                case MODE_NOT_SEEN:
                    (void) strcpy (fixBuff, "Fix not yet seen");
                    setColor (&NoFixColor);
                    break;

                case MODE_NO_FIX:
                    (void) strcpy (fixBuff, "No fix yet");
                    setColor (&NoFixColor);
                    break;

                case MODE_2D:
                    if ((gpsdata->set & LATLON_SET)) {
                        formatLat (gpsdata->fix.latitude);
                        formatLong (gpsdata->fix.longitude);
                        setColor (&TwoDFixColor);

                        if (verbose) {
                            (void) snprintf (fixBuff, STRINGBUFFSIZE,
                                             "2D %sFix, la %f, lo %f",
                                             fixStatus,
                                             gpsdata->fix.latitude,
                                             gpsdata->fix.longitude);
                        }
                    }
                    break;

                case MODE_3D:
                    if ((gpsdata->set & (LATLON_SET|ALTITUDE_SET))) {
                        formatLat (gpsdata->fix.latitude);
                        formatLong (gpsdata->fix.longitude);

                        /* Only if we have a fix are the details of it useful. Altitude is
                           only meaningful on a 3D fix. */
                        formatAltitude (gpsdata->fix.altitude);
                        setColor (&ThreeDFixColor);
                        if (verbose) {
                            (void) snprintf (fixBuff, STRINGBUFFSIZE,
                                             "3D %sFix, la %f, lo %f, %f",
                                             fixStatus,
                                             gpsdata->fix.latitude,
                                             gpsdata->fix.longitude,
                                             gpsdata->fix.altitude);
                        }

                    }
                    break;

                default:
                    (void) fprintf (stderr, "%s: Catastrophic error: Invalid mode %d.\n",
                                    baseName, gpsdata->fix.mode);
                    setColor (&NoFixColor);
                    return;
                } /* fix.mode */
            } /* MODE_SET */
            break;

        default:
            (void) fprintf (stderr, "Catastrophic error: Invalid status %d.\n",
                            gpsdata->status);
            setColor (&NoFixColor);
            return;
        }

        if (verbose) {
            (void) printf ("set 0x%08x, mode %d, %s, %s.\n",
                           gpsdata->set,
                           gpsdata->fix.mode,
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

    if (gps_waiting (&our_gps_data)) {
        errno = 0;
        if (gps_read (&our_gps_data) == -1) {
            if (errno == 0) {
                gtk_progress_bar_set_text (progress, "Lost contact with gpsd.");
            } else {
                gtk_progress_bar_set_text (progress, "Something went wrong.");
            }
            gtk_progress_bar_set_fraction (progress, 0.0);
            resynch ();
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

    (void) printf ("\n%s: Copyright © 2010 Charles Curley\n", baseName);

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
            (void) strcat (keyFileName, "/.");
            (void) strcat (keyFileName, baseName);

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
                            "%s,\nCompiled for gpsd API version %d.%d.\n",
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
                             gdk_pixbuf_new_from_inline ( -1, iconPixBuf,
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
