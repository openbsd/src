#ifndef LYSTRINGS_H
#define LYSTRINGS_H

#include <LYCurses.h>

#ifdef __cplusplus
extern "C" {
#endif
    typedef enum {
	NORECALL = 0
	,RECALL_URL
	,RECALL_CMD
	,RECALL_MAIL
    } RecallType;

#define is8bits(ch) (UCH(ch) >= 128)	/* isascii(ch) is not POSIX */

/*  UPPER8(ch1,ch2) is an extension of (TOUPPER(ch1) - TOUPPER(ch2))  */
    extern int UPPER8(int ch1,
		      int ch2);

    extern int get_mouse_link(void);
    extern int peek_mouse_link(void);
    extern int peek_mouse_levent(void);
    extern int fancy_mouse(WINDOW * win, int row, int *position);

    extern char *LYstrncpy(char *dst,
			   const char *src,
			   int n);
    extern void ena_csi(BOOLEAN flag);
    extern int get_popup_number(const char *msg,
				int *c,
				int *rel);
    extern int LYarrayLength(const char **list);
    extern int LYarrayWidth(const char **list);
    extern int LYgetch(void);
    extern int LYgetch_choice(void);
    extern int LYgetch_input(void);
    extern int LYgetch_single(void);
    extern int LYgetstr(char *inputline,
			int hidden,
			size_t bufsize,
			RecallType recall);
    extern char *LYstrsep(char **stringp,
			  const char *delim);
    extern char *LYstrstr(char *chptr,
			  const char *tarptr);
    extern char *LYmbcsstrncpy(char *dst,
			       const char *src,
			       int n_bytes,
			       int n_glyphs,
			       BOOL utf_flag);
    extern const char *LYmbcs_skip_glyphs(const char *data,
					  int n_glyphs,
					  BOOL utf_flag);
    extern int LYmbcsstrlen(const char *str,
			    BOOL utf_flag,
			    BOOL count_gcells);

    extern const char *LYno_attr_mbcs_strstr(const char *chptr,
					     const char *tarptr,
					     BOOL utf_flag,
					     BOOL count_gcells,
					     int *nstartp,
					     int *nendp);
    extern const char *LYno_attr_mbcs_case_strstr(const char *chptr,
						  const char *tarptr,
						  BOOL utf_flag,
						  BOOL count_gcells,
						  int *nstartp,
						  int *nendp);

#define non_empty(s) !isEmpty(s)

#define LYno_attr_mb_strstr(chptr, tarptr, utf_flag, count_gcells, nstartp, nendp) \
	(case_sensitive \
	    ? LYno_attr_mbcs_strstr(chptr, tarptr, utf_flag, count_gcells, nstartp, nendp) \
	    : LYno_attr_mbcs_case_strstr(chptr, tarptr, utf_flag, count_gcells, nstartp, nendp))

    extern const char *LYno_attr_char_strstr(const char *chptr,
					     const char *tarptr);
    extern const char *LYno_attr_char_case_strstr(const char *chptr,
						  const char *tarptr);

#define LYno_attr_strstr(chptr, tarptr) \
	(case_sensitive \
	? LYno_attr_char_strstr(chptr, tarptr) \
	: LYno_attr_char_case_strstr(chptr, tarptr))

    extern char *SNACopy(char **dest,
			 const char *src,
			 int n);
    extern char *SNACat(char **dest,
			const char *src,
			int n);

#define StrnAllocCopy(dest, src, n)  SNACopy (&(dest), src, n)
#define StrnAllocCat(dest, src, n)   SNACat  (&(dest), src, n)

    extern char *LYSafeGets(char **src, FILE *fp);

#ifdef EXP_CMD_LOGGING
    extern BOOL LYHaveCmdScript(void);
    extern int LYReadCmdKey(int mode);
    extern void LYCloseCmdLogfile(void);
    extern void LYOpenCmdLogfile(int argc, char **argv);
    extern void LYOpenCmdScript(void);
    extern void LYWriteCmdKey(int ch);

#else
#define LYHaveCmdScript() FALSE
#define LYReadCmdKey(mode) LYgetch_for(mode)
#define LYCloseCmdLogfile()	/* nothing */
#endif

/* values for LYgetch */
/* The following are lynxkeycodes, not to be confused with
   lynxactioncodes (LYK_*) to which they are often mapped.
   The lynxkeycodes include all single-byte keys as a subset. - kw
*/
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
#define MOUSE_KEY	285	/* 0x11D */
/*  ***** NOTES: *****
    If you add definitions for new lynxkeycodes to the above list that need to
    be mapped to LYK_* lynxactioncodes -

    - AT LEAST the tables keymap[] and key_override[] in LYKeymap.c have to be
      changed/reviewed, AS WELL AS the lineedit binding tables in LYEditmap.c !

    - KEYMAP_SIZE, defined in LYKeymap.h, may need to be changed !

    - See also table named_keys[] in LYKeymap.c for 'pretty' strings for the
      keys with codes >= 256 (to appear on the 'K'eymap page).  New keycodes
      should probably be assigned consecutively, so their key names can be
      easily added to named_keys[] (but see next point).  They should also be
      documented in lynx.cfg.

    - The DOS port uses its own native codes for some keys, unless they are
      remapped by the code in LYgetch().  See *.key files in docs/ directory. 
      Adding new keys here may conflict with those codes (affecting DOS users),
      unless/until remapping is added or changed in LYgetch().  (N)curses
      keypad codes (KEY_* from curses.h) can also directly appear as
      lynxkeycodes and conflict with our assignments, although that shouldn't
      happen - the useful ones should be recognized in LYgetch().

    - The actual recognition of raw input keys or escape sequences, and mapping
      to our lynxkeycodes, take place in LYgetch() and/or its subsidiary
      functions and/or the curses/slang/etc.  libraries.

    The basic lynxkeycodes can appear combined with various flags in
    higher-order bits as extended lynxkeycodes; see macros in LYKeymap.h.  The
    range of possible basic values is therefore limited, they have to be less
    than LKC_ISLKC (even if KEYMAP_SIZE is increased).
*/

#  define FOR_PANEL	0	/* normal screen, also LYgetch default */
#  define FOR_CHOICE	1	/* mouse menu */
#  define FOR_INPUT	2	/* form input and textarea field */
#  define FOR_PROMPT	3	/* string prompt editing */
#  define FOR_SINGLEKEY	4	/* single key prompt, confirmation */

#define VISIBLE  0
#define HIDDEN   1

#ifdef EXP_ALT_BINDINGS
/*  Enable code implementing additional, mostly emacs-like, line-editing
    functions. - kw */
#define ENHANCED_LINEEDIT
#endif

/* EditFieldData preserves state between calls to LYEdit1
 */
    typedef struct _EditFieldData {

	int sx;			/* Origin of editfield                       */
	int sy;
	int dspwdth;		/* Screen real estate for editting           */

	int strlen;		/* Current size of string.                   */
	int maxlen;		/* Max size of string, excluding zero at end */
	char pad;		/* Right padding  typically ' ' or '_'       */
	BOOL hidden;		/* Masked password entry flag                */

	BOOL dirty;		/* accumulate refresh requests               */
	BOOL panon;		/* Need horizontal scroll indicator          */
	int xpan;		/* Horizontal scroll offset                  */
	int pos;		/* Insertion point in string                 */
	int margin;		/* Number of columns look-ahead/look-back    */
	int current_modifiers;	/* Modifiers for next input lynxkeycode */
#ifdef ENHANCED_LINEEDIT
	int mark;		/* position of emacs-like mark, or -1-pos to denote
				   unactive mark.  */
#endif

	char buffer[1024];	/* String buffer                          */

    } EditFieldData;

/* line-edit action encoding */

    typedef enum {
	LYE_NOP = 0		/* Do Nothing            */
	,LYE_CHAR		/* Insert printable char */
	,LYE_ENTER		/* Input complete, return char/lynxkeycode */
	,LYE_TAB		/* Input complete, return TAB  */
	,LYE_STOP		/* Input complete, deactivate  */
	,LYE_ABORT		/* Input cancelled       */

	,LYE_FORM_PASS		/* In form fields: input complete,
				   return char / lynxkeycode;
				   Elsewhere: Do Nothing */

	,LYE_DELN		/* Delete next/curr char */
	,LYE_DELC		/* Obsolete (DELC case was equiv to DELN) */
	,LYE_DELP		/* Delete prev      char */
	,LYE_DELNW		/* Delete next word      */
	,LYE_DELPW		/* Delete prev word      */

	,LYE_ERASE		/* Erase the line        */

	,LYE_BOL		/* Go to begin of line   */
	,LYE_EOL		/* Go to end   of line   */
	,LYE_FORW		/* Cursor forwards       */
	,LYE_FORW_RL		/* Cursor forwards or right link */
	,LYE_BACK		/* Cursor backwards      */
	,LYE_BACK_LL		/* Cursor backwards or left link */
	,LYE_FORWW		/* Word forward          */
	,LYE_BACKW		/* Word back             */

	,LYE_LOWER		/* Lower case the line   */
	,LYE_UPPER		/* Upper case the line   */

	,LYE_LKCMD		/* Invoke command prompt */

	,LYE_AIX		/* Hex 97                */

	,LYE_DELBL		/* Delete back to BOL    */
	,LYE_DELEL		/* Delete thru EOL       */

	,LYE_SWMAP		/* Switch input keymap   */

	,LYE_TPOS		/* Transpose characters  */

	,LYE_SETM1		/* Set modifier 1 flag   */
	,LYE_SETM2		/* Set modifier 2 flag   */
	,LYE_UNMOD		/* Fall back to no-modifier command */

	,LYE_C1CHAR		/* Insert C1 char if printable */

	,LYE_SETMARK		/* emacs-like set-mark-command */
	,LYE_XPMARK		/* emacs-like exchange-point-and-mark */
	,LYE_KILLREG		/* emacs-like kill-region */
	,LYE_YANK		/* emacs-like yank */
#ifdef CAN_CUT_AND_PASTE
	,LYE_PASTE		/* ClipBoard to Lynx       */
#endif
    } LYEditCodes;

/* All preceding values must be within 0x00..0x7f - kw */

/*  The following are meant to be bitwise or-ed:  */
#define LYE_DF       0x80	/* Flag to set modifier 3 AND do other
				   action */
#define LYE_FORM_LAC 0x1000	/* Flag to pass lynxactioncode given by
				   lower bits.  Doesn't fit in a char! */

#if defined(USE_KEYMAPS)
    extern int lynx_initialize_keymaps(void);
    extern int map_string_to_keysym(const char *src, int *lec);
#endif

    extern char *LYElideString(char *str,
			       int cut_pos);
    extern void LYEscapeStartfile(char **buffer);
    extern void LYLowerCase(char *buffer);
    extern void LYUpperCase(char *buffer);
    extern BOOLEAN LYRemoveNewlines(char *buffer);
    extern char *LYReduceBlanks(char *buffer);
    extern char *LYRemoveBlanks(char *buffer);
    extern char *LYSkipBlanks(char *buffer);
    extern char *LYSkipNonBlanks(char *buffer);
    extern const char *LYSkipCBlanks(const char *buffer);
    extern const char *LYSkipCNonBlanks(const char *buffer);
    extern void LYTrimLeading(char *buffer);
    extern char *LYTrimNewline(char *buffer);
    extern void LYTrimTrailing(char *buffer);
    extern void LYTrimAllStartfile(char *buffer);
    extern BOOLEAN LYTrimStartfile(char *buffer);
    extern void LYSetupEdit(EditFieldData *edit, char *old,
			    int maxstr,
			    int maxdsp);
    extern void LYRefreshEdit(EditFieldData *edit);
    extern int EditBinding(int ch);	/* in LYEditmap.c */
    extern BOOL LYRemapEditBinding(int xlkc,
				   int lec,
				   int select_edi);	/* in LYEditmap.c */
    extern int LYKeyForEditAction(int lec);	/* in LYEditmap.c */
    extern int LYEditKeyForAction(int lac, int *pmodkey);	/* LYEditmap.c */
    extern int LYEdit1(EditFieldData *edit, int ch,
		       int action,
		       BOOL maxMessage);
    extern void LYCloseCloset(RecallType recall);
    extern int LYhandlePopupList(int cur_choice,
				 int ly,
				 int lx,
				 const char **choices,
				 int width,
				 int i_length,
				 int disabled,
				 BOOLEAN for_mouse,
				 BOOLEAN numbered);

    typedef unsigned char LYEditCode;

    extern int current_lineedit;
    extern const char *LYLineeditNames[];
    extern LYEditCode *LYLineEditors[];
    extern const char *LYLineeditHelpURLs[];

    extern const char *LYLineeditHelpURL(void);

    extern int escape_bound;

#define LYLineEdit(e,c,m) LYEdit1(e, c, EditBinding(c) & ~LYE_DF, m)

/* Dummy initializer for LYEditmap.c */
    extern int LYEditmapDeclared(void);

    extern int LYEditInsert(EditFieldData *edit,
			    unsigned const char *s,
			    int len, int map_active,
			    BOOL maxMessage);

#ifdef __cplusplus
}
#endif
#endif				/* LYSTRINGS_H */
