/* LYEditMap.c
   Keybindings for line and form editting.
*/

#include "HTUtils.h"
#include "tcp.h"
#include "LYStrings.h"

PUBLIC int current_lineedit = 0;  /* Index into LYLineEditors[]   */

/*
 * See LYStrings.h for the LYE definitions.
 */
PRIVATE char DefaultEditBinding[]={

LYE_NOP,        LYE_BOL,        LYE_DELPW,      LYE_ABORT,
/* nul          ^A              ^B              ^C      */

LYE_DELC,       LYE_EOL,        LYE_DELNW,      LYE_ABORT,
/* ^D           ^E              ^F              ^G      */

LYE_DELP,       LYE_ENTER,      LYE_ENTER,      LYE_LOWER,
/* bs           tab             nl              ^K      */

LYE_NOP,        LYE_ENTER,      LYE_FORWW,      LYE_ABORT,
/* ^L           cr              ^N              ^O      */

LYE_BACKW,      LYE_NOP,        LYE_DELN,       LYE_NOP,
/* ^P           XON             ^R              XOFF    */

LYE_UPPER,      LYE_ERASE,      LYE_LKCMD,      LYE_NOP,
/* ^T           ^U              ^V              ^W      */

LYE_ERASE,      LYE_NOP,        LYE_NOP,        LYE_NOP,
/* ^X           ^Y              ^Z              ESC     */

LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
/* ^\           ^]              ^^              ^_      */

/* sp .. RUBOUT                                         */
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_DELP,

/* 80..9F ISO-8859-1 8-bit escape characters. */
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_AIX,
/*                                               97 AIX    */
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,

/* A0..FF (permissible ISO-8859-1) 8-bit characters. */
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,
LYE_CHAR,       LYE_CHAR,       LYE_CHAR,       LYE_CHAR,

/* 100..10E function key definitions in LYStrings.h */
LYE_NOP,        LYE_NOP,        LYE_FORW,       LYE_BACK,
/* UPARROW      DNARROW         RTARROW         LTARROW     */

LYE_NOP,        LYE_NOP,        LYE_BOL,        LYE_EOL,
/* PGDOWN       PGUP            HOME            END         */

LYE_NOP,        LYE_TAB,        LYE_BOL,        LYE_EOL,
/* F1           Do key          Find key        Select key  */

LYE_NOP,        LYE_DELP,       LYE_NOP,        LYE_NOP,
/* Insert key   Remove key      DO_NOTHING      ...         */
};

/*
 * Add your favorite key binding HERE
 */


/*
 * Add the array name to LYLineEditors
 */

PUBLIC char * LYLineEditors[]={
        DefaultEditBinding,     /* You can't please everyone, so you ... DW */
};

/*
 * Add the name that the user will see below.
 * The order of LYLineEditors and LyLineditNames MUST be the same
 */
PUBLIC char * LYLineeditNames[]={
        "Default Binding     ",
        (char *) 0
};

/*
 * Dummy initializer to ensure this module is linked
 * if the external model is common block, and the
 * module is ever placed in a library. - FM
 */
PUBLIC int LYEditmapDeclared NOPARAMS
{
    int status = 1;

    return status;
}

