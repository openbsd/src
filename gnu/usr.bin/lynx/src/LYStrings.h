/*
 * $LynxId: LYStrings.h,v 1.113 2013/10/20 20:33:23 tom Exp $
 */
#ifndef LYSTRINGS_H
#define LYSTRINGS_H

#include <LYCurses.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SQUOTE '\''
#define DQUOTE '"'
#define ESCAPE '\\'
#define LPAREN '('
#define RPAREN ')'

    typedef const char *const Const2CharPtr;
    typedef enum {
	NORECALL = 0
	,RECALL_URL
	,RECALL_CMD
	,RECALL_MAIL
    } RecallType;

#define IS_UTF8_TTY (BOOLEAN) (LYCharSet_UC[current_char_set].enc == UCT_ENC_UTF8)
#define IS_CJK_TTY  (BOOLEAN) (HTCJK != NOCJK)

#define is8bits(ch) (BOOLEAN) (UCH(ch) >= 128)	/* isascii(ch) is not POSIX */

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
#define LYStrNCpy(dst,src,n) LYstrncpy(dst,src,(int)(n))
    extern void ena_csi(int flag);
    extern int get_popup_number(const char *msg,
				int *c,
				int *rel);
    extern int LYarrayLength(STRING2PTR list);
    extern int LYarrayWidth(STRING2PTR list);
    extern int LYgetch(void);
    extern int LYgetch_choice(void);
    extern int LYgetch_input(void);
    extern int LYgetch_single(void);
    extern int LYgetstr(char *inputline,
			int masked,
			size_t bufsize,
			RecallType recall);
#define LYGetStr(input,masked,bufsize,recall) \
	LYgetstr(input,masked,(size_t)(bufsize),recall)
    extern int LYgetBString(bstring **inputline,
			    int masked,
			    size_t max_cols,
			    RecallType recall);
    extern int LYscanFloat(const char *source, float *result);
    extern int LYscanFloat2(const char **source, float *result);
    extern char *LYstrsep(char **stringp,
			  const char *delim);
    extern char *LYstrstr(char *chptr,
			  const char *tarptr);
    extern char *LYmbcsstrncpy(char *dst,
			       const char *src,
			       int n_bytes,
			       int n_glyphs,
			       int utf_flag);
    extern const char *LYmbcs_skip_cells(const char *data,
					 int n_cells,
					 int utf_flag);
    extern const char *LYmbcs_skip_glyphs(const char *data,
					  int n_glyphs,
					  int utf_flag);
    extern int LYmbcsstrlen(const char *str,
			    int utf_flag,
			    int count_gcells);

    extern const char *LYno_attr_mbcs_strstr(const char *chptr,
					     const char *tarptr,
					     int utf_flag,
					     int count_gcells,
					     int *nstartp,
					     int *nendp);
    extern const char *LYno_attr_mbcs_case_strstr(const char *chptr,
						  const char *tarptr,
						  int utf_flag,
						  int count_gcells,
						  int *nstartp,
						  int *nendp);

#define LYno_attr_mb_strstr(chptr, tarptr, utf_flag, count_gcells, nstartp, nendp) \
	(LYcase_sensitive \
	    ? LYno_attr_mbcs_strstr(chptr, tarptr, utf_flag, count_gcells, nstartp, nendp) \
	    : LYno_attr_mbcs_case_strstr(chptr, tarptr, utf_flag, count_gcells, nstartp, nendp))

    extern const char *LYno_attr_char_strstr(const char *chptr,
					     const char *tarptr);
    extern const char *LYno_attr_char_case_strstr(const char *chptr,
						  const char *tarptr);

#define LYno_attr_strstr(chptr, tarptr) \
	(LYcase_sensitive \
	? LYno_attr_char_strstr(chptr, tarptr) \
	: LYno_attr_char_case_strstr(chptr, tarptr))

    extern char *SNACopy(char **dest,
			 const char *src,
			 size_t n);
    extern char *SNACat(char **dest,
			const char *src,
			size_t n);

#define StrnAllocCopy(dest, src, n)  SNACopy (&(dest), src, n)
#define StrnAllocCat(dest, src, n)   SNACat  (&(dest), src, n)

    extern char *LYSafeGets(char **src, FILE *fp);

#ifdef USE_CMD_LOGGING
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
     * lynxactioncodes (LYK_*) to which they are often mapped.
     * The lynxkeycodes include all single-byte keys as a subset.
     * These are "extra" keys which do not fit into a single byte.
     */
    typedef enum {
	UNKNOWN_KEY = -1
	,UPARROW_KEY = 256
	,DNARROW_KEY
	,RTARROW_KEY
	,LTARROW_KEY
	,PGDOWN_KEY
	,PGUP_KEY
	,HOME_KEY
	,END_KEY
	,F1_KEY
	,DO_KEY
	,FIND_KEY
	,SELECT_KEY
	,INSERT_KEY
	,REMOVE_KEY
	,DO_NOTHING
	,BACKTAB_KEY
	/* these should be referenced by name in keymap, e.g., "f2" */
	,F2_KEY
	,F3_KEY
	,F4_KEY
	,F5_KEY
	,F6_KEY
	,F7_KEY
	,F8_KEY
	,F9_KEY
	,F10_KEY
	,F11_KEY
	,F12_KEY
	/* this has known value */
	,MOUSE_KEY = 285	/* 0x11D */
    } LYExtraKeys;

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

#ifdef USE_ALT_BINDINGS
/*  Enable code implementing additional, mostly emacs-like, line-editing
    functions. - kw */
#define ENHANCED_LINEEDIT
#endif

/* FieldEditor preserves state between calls to LYDoEdit
 */
    typedef struct {

	int efStartX;		/* Origin of edit-field                      */
	int efStartY;
	int efWidth;		/* Screen real estate for editing            */

	char *efBuffer;		/* the buffer which is being edited */
	size_t efBufInUse;	/* current size of string.                   */
	size_t efBufAlloc;	/* current buffer-size, excluding nul at end */
	size_t efBufLimit;	/* buffer size limit, zero if indefinite     */

	char efPadChar;		/* Right padding  typically ' ' or '_'       */
	BOOL efIsMasked;	/* Masked password entry flag                */

	BOOL efIsDirty;		/* accumulate refresh requests               */
	BOOL efIsPanned;	/* Need horizontal scroll indicator          */
	int efDpyStart;		/* Horizontal scroll offset                  */
	int efEditAt;		/* Insertion point in string                 */
	int efPanMargin;	/* Number of columns look-ahead/look-back    */
	int efInputMods;	/* Modifiers for next input lynxkeycode */
#ifdef ENHANCED_LINEEDIT
	int efEditMark;		/* position of emacs-like mark, or -1-pos to denote
				   unactive mark.  */
#endif

	int *efOffs2Col;	/* fixups for multibyte characters */

    } FieldEditor;

/* line-edit action encoding */

    typedef enum {
	LYE_UNKNOWN = -1	/* no binding            */
	,LYE_NOP = 0		/* Do Nothing            */
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

    extern BOOL LYRemapEditBinding(int xlkc, int lec, int select_edi);	/* in LYEditmap.c */
    extern BOOLEAN LYRemoveNewlines(char *buffer);
    extern BOOLEAN LYTrimStartfile(char *buffer);
    extern LYExtraKeys LYnameToExtraKeys(const char *name);
    extern char *LYElideString(char *str, int cut_pos);
    extern char *LYReduceBlanks(char *buffer);
    extern char *LYRemoveBlanks(char *buffer);
    extern char *LYSkipBlanks(char *buffer);
    extern char *LYSkipNonBlanks(char *buffer);
    extern char *LYTrimNewline(char *buffer);
    extern const char *LYSkipCBlanks(const char *buffer);
    extern const char *LYSkipCNonBlanks(const char *buffer);
    extern const char *LYextraKeysToName(LYExtraKeys code);
    extern int EditBinding(int ch);	/* in LYEditmap.c */
    extern int LYDoEdit(FieldEditor * edit, int ch, int action, int maxMessage);
    extern int LYEditKeyForAction(int lac, int *pmodkey);	/* LYEditmap.c */
    extern int LYKeyForEditAction(int lec);	/* in LYEditmap.c */
    extern int LYhandlePopupList(int cur_choice, int ly, int lx,
				 STRING2PTR choices,
				 int width,
				 int i_length,
				 int disabled,
				 int for_mouse);
    extern void LYCloseCloset(RecallType recall);
    extern void LYEscapeStartfile(char **buffer);
    extern void LYFinishEdit(FieldEditor * edit);
    extern void LYLowerCase(char *buffer);
    extern void LYRefreshEdit(FieldEditor * edit);
    extern void LYSetupEdit(FieldEditor * edit, char *old,
			    size_t buffer_limit,
			    int display_limit);
    extern void LYTrimAllStartfile(char *buffer);
    extern void LYTrimLeading(char *buffer);
    extern void LYTrimTrailing(char *buffer);
    extern void LYUpperCase(char *buffer);

    typedef short LYEditCode;

    typedef struct {
	int code;
	LYEditCode edit;
    } LYEditInit;

    typedef struct {
	const char *name;
	const LYEditInit *init;
	LYEditCode *used;
    } LYEditConfig;

    extern int current_lineedit;
    extern const char *LYEditorNames[];
    extern LYEditConfig LYLineEditors[];
    extern const char *LYLineeditHelpURLs[];

#define CurrentLineEditor() LYLineEditors[current_lineedit].used

    extern void LYinitEditmap(void);
    extern void LYinitKeymap(void);
    extern const char *LYLineeditHelpURL(void);

    extern int escape_bound;

#define LYLineEdit(e,c,m) LYDoEdit(e, c, EditBinding(c) & ~LYE_DF, m)

    extern int LYEditInsert(FieldEditor * edit, unsigned const char *s,
			    int len, int map_active,
			    int maxMessage);

#ifdef __cplusplus
}
#endif
#endif				/* LYSTRINGS_H */
