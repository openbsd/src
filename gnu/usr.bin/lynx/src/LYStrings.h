#ifndef LYSTRINGS_H
#define LYSTRINGS_H

#include <LYCurses.h>

/*  UPPER8(ch1,ch2) is an extension of (TOUPPER(ch1) - TOUPPER(ch2))  */
extern int UPPER8  PARAMS((
	int		ch1,
	int		ch2));

extern int get_mouse_link NOPARAMS;
extern int peek_mouse_link NOPARAMS;
extern int peek_mouse_levent NOPARAMS;
extern int fancy_mouse PARAMS((WINDOW *win, int row, int *position));

#ifdef HAVE_STRERROR
#define LYStrerror strerror
#else
extern char *LYStrerror PARAMS((
	int		code));
#endif /* HAVE_STRERROR */

extern char * LYstrncpy PARAMS((
	char *		dst,
	CONST char *	src,
	int		n));
extern void ena_csi PARAMS((BOOLEAN flag));
extern int LYgetch NOPARAMS;
extern int LYgetch_for PARAMS((
	int		code));
extern int LYgetstr PARAMS((
	char *		inputline,
	int		hidden,
	size_t		bufsize,
	int		recall));
extern char *LYstrsep PARAMS((
	char **		stringp,
	CONST char *	delim));
extern char * LYstrstr PARAMS((
	char *		chptr,
	CONST char *	tarptr));
extern char * LYmbcsstrncpy PARAMS((
	char *		dst,
	CONST char *	src,
	int		n_bytes,
	int		n_glyphs,
	BOOL		utf_flag));
extern char * LYmbcs_skip_glyphs PARAMS((
	char *		data,
	int		n_glyphs,
	BOOL		utf_flag));
extern int LYmbcsstrlen PARAMS((
	char *		str,
	BOOL		utf_flag));
extern char * LYno_attr_mbcs_strstr PARAMS((
	char *		chptr,
	char *		tarptr,
	BOOL		utf_flag,
	int *		nstartp,
	int *		nendp));
extern char * LYno_attr_mbcs_case_strstr PARAMS((
	char *		chptr,
	char *		tarptr,
	BOOL		utf_flag,
	int *		nstartp,
	int *		nendp));
extern char * LYno_attr_char_strstr PARAMS((
	char *		chptr,
	char *		tarptr));
extern char * LYno_attr_char_case_strstr PARAMS((
	char *		chptr,
	char *		tarptr));

extern char * SNACopy PARAMS((
	char **		dest,
	CONST char *	src,
	int		n));
extern char * SNACat PARAMS((
	char **		dest,
	CONST char *	src,
	int		n));
#define StrnAllocCopy(dest, src, n)  SNACopy (&(dest), src, n)
#define StrnAllocCat(dest, src, n)   SNACat  (&(dest), src, n)

extern char *LYSafeGets PARAMS((char ** src, FILE * fp));

/* values for LYgetch */
#define UPARROW		256	/* 0x100 */
#define DNARROW		257	/* 0x101 */
#define RTARROW		258	/* 0x102 */
#define LTARROW		259	/* 0x103 */
#define PGDOWN		260	/* 0x104 */
#define PGUP		261	/* 0x105 */
#define HOME		262	/* 0x106 */
#define END_KEY		263	/* 0x107 */
#define F1		264	/* 0x108 */
#define DO_KEY		265	/* 0x109 */
#define FIND_KEY	266	/* 0x10A */
#define SELECT_KEY	267	/* 0x10B */
#define INSERT_KEY	268	/* 0x10C */
#define REMOVE_KEY	269	/* 0x10D */
#define DO_NOTHING	270	/* 0x10E */
#define BACKTAB_KEY	271	/* 0x10F */
#define MOUSE_KEY	0x11d	/* 0x11D */
/*  *** NOTE: ***
    If you add definitions for new lynxkeycodes to the above list that
    need to be mapped to LYK_* lynxactioncodes -
    - AT LEAST the tables keymap[] and key_override[] in LYKeymap.c
      have to be changed/reviewed, AS WELL AS the lineedit binding
      tables in LYEditmap.c !
    - KEYMAP_SIZE, defined in LYKeymap.h, may need to be changed !
*/


#  define FOR_PANEL	0
#  define FOR_CHOICE	1
#  define FOR_INPUT	2

#define VISIBLE  0
#define HIDDEN   1
#define NORECALL 0
#define RECALL   1

/* EditFieldData preserves state between calls to LYEdit1
 */
typedef struct _EditFieldData {

        int  sx;        /* Origin of editfield                       */
        int  sy;
        int  dspwdth;   /* Screen real estate for editting           */

        int  strlen;    /* Current size of string.                   */
        int  maxlen;    /* Max size of string, excluding zero at end */
        char pad;       /* Right padding  typically ' ' or '_'       */
        BOOL hidden;    /* Masked password entry flag                */

        BOOL dirty;     /* accumulate refresh requests               */
        BOOL panon;     /* Need horizontal scroll indicator          */
        int  xpan;      /* Horizontal scroll offset                  */
        int  pos;       /* Insertion point in string                 */
        int  margin;    /* Number of columns look-ahead/look-back    */

        char buffer[1024]; /* String buffer                          */

} EditFieldData;

/* line-edit action encoding */

#define LYE_NOP 0		  /* Do Nothing            */
#define LYE_CHAR  (LYE_NOP   +1)  /* Insert printable char */
#define LYE_ENTER (LYE_CHAR  +1)  /* Input complete, return char/lynxkeycode */
#define LYE_TAB   (LYE_ENTER +1)  /* Input complete, return TAB  */
#define LYE_ABORT (LYE_TAB   +1)  /* Input cancelled       */

#define LYE_FORM_PASS (LYE_ABORT +1)  /* In form fields: input complete,
					 return char / lynxkeycode;
					 Elsewhere: Do Nothing */

#define LYE_DELN  (LYE_FORM_PASS +1)  /* Delete next/curr char */
#define LYE_DELC  (LYE_DELN)      /* Obsolete (DELC case was equiv to DELN) */
#define LYE_DELP  (LYE_DELN  +1)  /* Delete prev      char */
#define LYE_DELNW (LYE_DELP  +1)  /* Delete next word      */
#define LYE_DELPW (LYE_DELNW +1)  /* Delete prev word      */

#define LYE_ERASE (LYE_DELPW +1)  /* Erase the line        */

#define LYE_BOL   (LYE_ERASE +1)  /* Go to begin of line   */
#define LYE_EOL   (LYE_BOL   +1)  /* Go to end   of line   */
#define LYE_FORW  (LYE_EOL   +1)  /* Cursor forwards       */
#define LYE_BACK  (LYE_FORW  +1)  /* Cursor backwards      */
#define LYE_FORWW (LYE_BACK  +1)  /* Word forward          */
#define LYE_BACKW (LYE_FORWW +1)  /* Word back             */

#define LYE_LOWER (LYE_BACKW +1)  /* Lower case the line   */
#define LYE_UPPER (LYE_LOWER +1)  /* Upper case the line   */

#define LYE_LKCMD (LYE_UPPER +1)  /* Invoke command prompt */

#define LYE_AIX   (LYE_LKCMD +1)  /* Hex 97                */

#define LYE_DELBL (LYE_AIX   +1)  /* Delete back to BOL    */
#define LYE_DELEL (LYE_DELBL +1)  /* Delete thru EOL       */

#define LYE_SWMAP (LYE_DELEL +1)  /* Switch input keymap   */

#if defined(USE_KEYMAPS)
extern int lynx_initialize_keymaps NOPARAMS;
#endif

extern void LYLowerCase PARAMS((
	char *		buffer));
extern void LYUpperCase PARAMS((
	char *		buffer));
extern void LYRemoveBlanks PARAMS((
	char *		buffer));
extern char * LYSkipBlanks PARAMS((
	char *		buffer));
extern char * LYSkipNonBlanks PARAMS((
	char *		buffer));
extern CONST char * LYSkipCBlanks PARAMS((
	CONST char *	buffer));
extern CONST char * LYSkipCNonBlanks PARAMS((
	CONST char *	buffer));
extern void LYTrimLeading PARAMS((
	char *		buffer));
extern void LYTrimTrailing PARAMS((
	char *		buffer));
extern BOOLEAN LYTrimStartfile PARAMS((
	char *		buffer));
extern void LYSetupEdit PARAMS((
	EditFieldData *	edit,
	char *		old,
	int		maxstr,
	int		maxdsp));
extern void LYRefreshEdit PARAMS((
	EditFieldData *	edit));
extern int EditBinding PARAMS((int ch));
extern int LYEdit1 PARAMS((
	EditFieldData *	edit,
	int		ch,
	int		action,
	BOOL		maxMessage));
extern void LYOpenCloset NOPARAMS;
extern void LYCloseCloset NOPARAMS;

extern int current_lineedit;
extern char * LYLineeditNames[];
extern char * LYLineEditors[];

/* Push a character through the lineedit machinery */
#ifdef    NOT_ASCII  /* S/390 -- gil -- 2080 */
#define EditBinding(c) (LYLineEditors[current_lineedit][(c)<256 ? TOASCII(c) : c])
#else  /* NOT_ASCII */
#define EditBinding(c) (LYLineEditors[current_lineedit][c])
#endif /* NOT_ASCII */
#define LYLineEdit(e,c,m) LYEdit1(e,c,EditBinding(c),m)

/* Dummy initializer for LYEditmap.c */
extern int LYEditmapDeclared NOPARAMS;

#endif /* LYSTRINGS_H */
