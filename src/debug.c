/* A function for debugging only. */

#include "debug.h"          /* prototypes and other goodies. */


void showStatusStrings (void) {
    for (gint i=-1; i < SIZESTATUSSTRINGS-1; i++) {
        printf ("String %d: %s.\n", i, getStatusString (i));
    }
}
