/* LYEditMap.c
   Keybindings for line and form editting.
*/

#include <HTUtils.h>
#include <LYStrings.h>
#include <LYKeymap.h>		/* only for KEYMAP_SIZE - kw */

PUBLIC int current_lineedit = 0;  /* Index into LYLineEditors[]   */

/*
 * See LYStrings.h for the LYE definitions.
 */
PRIVATE char DefaultEditBinding[KEYMAP_SIZE-1]={

LYE_NOP,        LYE_BOL,        LYE_DELPW,      LYE_ABORT,
/* nul          ^A              ^B              ^C      */

LYE_DELN,       LYE_EOL,        LYE_DELNW,      LYE_ABORT,
/* ^D           ^E              ^F              ^G      */

LYE_DELP,       LYE_TAB,      LYE_ENTER,      LYE_LOWER,
/* bs           tab             nl              ^K      */

LYE_NOP,        LYE_ENTER,      LYE_FORWW,      LYE_ABORT,
/* ^L           cr              ^N              ^O      */

LYE_BACKW,      LYE_NOP,        LYE_DELN,       LYE_NOP,
/* ^P           XON             ^R              XOFF    */

LYE_UPPER,      LYE_ERASE,      LYE_LKCMD,      LYE_NOP,
/* ^T           ^U              ^V              ^W      */

LYE_ERASE,      LYE_NOP,        LYE_NOP,        LYE_NOP,
/* ^X           ^Y              ^Z              ESC     */

LYE_NOP,        LYE_NOP,        LYE_SWMAP,      LYE_DELEL,
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

/* 100..10F function key definitions in LYStrings.h */
LYE_FORM_PASS,  LYE_FORM_PASS,  LYE_FORW,       LYE_BACK,
/* UPARROW      DNARROW         RTARROW         LTARROW     */

LYE_FORM_PASS,  LYE_FORM_PASS,  LYE_BOL,        LYE_EOL,
/* PGDOWN       PGUP            HOME            END         */

#if (defined(_WINDOWS) || defined(__DJGPP__))

LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
/* F1 */

#else

LYE_NOP,        LYE_TAB,        LYE_BOL,        LYE_EOL,
/* F1           Do key          Find key        Select key  */

#endif /* _WINDOWS || __DJGPP__ */

LYE_NOP,        LYE_DELP,       LYE_NOP,        LYE_FORM_PASS,
/* Insert key   Remove key      DO_NOTHING      Back tab */

/* 110..18F */
#if (defined(_WINDOWS) || defined(__DJGPP__)) && defined(USE_SLANG) && !defined(DJGPP_KEYHANDLER)

LYE_DELP,       LYE_ENTER,      LYE_NOP,        LYE_NOP,
/* Backspace    Enter */

#else

LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,

#endif /* USE_SLANG &&(_WINDOWS || __DJGPP) && !DJGPP_KEYHANDLER */

LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
/*             MOUSE_KEY  */
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
/* 190..20F */

LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
/* 210..28F */

LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
/* 290..293 */
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
};

/*
 * Add your favorite key bindings HERE
 */

/* KED-01 */ /* Default except: ^B=cursor-backward,  ^F=cursor-forward,   */
             /*                 ^K=delete-to-EOL,    ^X=delete-to-BOL,    */
             /*                 ^R=delete-prev-word, ^T=delete-next-word, */
             /*                 ^^=upper-case-line,  ^_=lower-case-line   */
/* Why the difference for tab? - kw */

#ifdef EXP_ALT_BINDINGS
PRIVATE char BetterEditBinding[KEYMAP_SIZE-1]={

LYE_NOP,        LYE_BOL,        LYE_BACK,       LYE_ABORT,
/* nul          ^A              ^B              ^C      */

LYE_DELN,       LYE_EOL,        LYE_FORW,       LYE_ABORT,
/* ^D           ^E              ^F              ^G      */

LYE_DELP,       LYE_ENTER,      LYE_ENTER,      LYE_DELEL,
/* bs           tab             nl              ^K      */

LYE_NOP,        LYE_ENTER,      LYE_FORWW,      LYE_ABORT,
/* ^L           cr              ^N              ^O      */

LYE_BACKW,      LYE_NOP,        LYE_DELPW,      LYE_NOP,
/* ^P           XON             ^R              XOFF    */

LYE_DELNW,      LYE_ERASE,      LYE_LKCMD,      LYE_NOP,
/* ^T           ^U              ^V              ^W      */

LYE_DELBL,      LYE_NOP,        LYE_NOP,        LYE_NOP,
/* ^X           ^Y              ^Z              ESC     */

LYE_NOP,        LYE_NOP,        LYE_UPPER,      LYE_LOWER,
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
LYE_FORM_PASS,  LYE_FORM_PASS,  LYE_FORW,       LYE_BACK,
/* UPARROW      DNARROW         RTARROW         LTARROW     */

LYE_FORM_PASS,  LYE_FORM_PASS,  LYE_BOL,        LYE_EOL,
/* PGDOWN       PGUP            HOME            END         */

#if (defined(_WINDOWS) || defined(__DJGPP__))

LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
/* F1 */

#else

LYE_NOP,        LYE_TAB,        LYE_BOL,        LYE_EOL,
/* F1           Do key          Find key        Select key  */

#endif /* _WINDOWS || __DJGPP__ */

LYE_NOP,        LYE_DELP,       LYE_NOP,        LYE_FORM_PASS,
/* Insert key   Remove key      DO_NOTHING      Back tab */

/* 110..18F */
#if (defined(_WINDOWS) || defined(__DJGPP__)) && defined(USE_SLANG) && !defined(DJGPP_KEYHANDLER)

LYE_DELP,       LYE_ENTER,      LYE_NOP,        LYE_NOP,
/* Backspace    Enter */

#else

LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,

#endif /* USE_SLANG &&(_WINDOWS || __DJGPP) && !DJGPP_KEYHANDLER */

LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
/*             MOUSE_KEY  */
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
/* 190..20F */

LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
/* 210..28F */

LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
/* 290..293 */
LYE_NOP,        LYE_NOP,        LYE_NOP,        LYE_NOP,
};
#endif


/*
 * Add the array name to LYLineEditors
 */

PUBLIC char * LYLineEditors[]={
        DefaultEditBinding,     /* You can't please everyone, so you ... DW */
#ifdef EXP_ALT_BINDINGS
	BetterEditBinding,      /* No, you certainly can't ... /ked 10/27/98*/
#endif
};

/*
 * Add the name that the user will see below.
 * The order of LYLineEditors and LyLineditNames MUST be the same
 */
PUBLIC char * LYLineeditNames[]={
	"Default Binding",
#ifdef EXP_ALT_BINDINGS
	"Alternate Bindings",
#endif
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

