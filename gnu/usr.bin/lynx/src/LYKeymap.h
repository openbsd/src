#ifndef LYKEYMAP_H
#define LYKEYMAP_H

#include <HTUtils.h>
#include <HTList.h>
#include <LYCurses.h>

extern BOOLEAN LYisNonAlnumKeyname PARAMS((int ch, int KeyName));
extern HTList *LYcommandList NOPARAMS;
extern char *LYKeycodeToString PARAMS((int c, BOOLEAN upper8));
extern char *fmt_keys PARAMS((int lkc_first, int lkc_second));
extern char *key_for_func PARAMS((int func));
extern char *key_for_func_ext PARAMS((int lac, int context_code));
extern int LYReverseKeymap PARAMS((int KeyName));
extern int LYStringToKeycode PARAMS((char *src));
extern int lacname_to_lac PARAMS((CONST char *func));
extern int lecname_to_lec PARAMS((CONST char *func));
extern int lkcstring_to_lkc PARAMS((CONST char *src));
extern int remap PARAMS((char *key, char *func, BOOLEAN for_dired));
extern void print_keymap PARAMS((char **newfile));
extern void reset_emacs_keys NOPARAMS;
extern void reset_numbers_as_arrows NOPARAMS;
extern void reset_vi_keys NOPARAMS;
extern void set_emacs_keys NOPARAMS;
extern void set_numbers_as_arrows NOPARAMS;
extern void set_vi_keys NOPARAMS;
extern void set_vms_keys NOPARAMS;

/* We only use unsigned keycodes; if there's a problem matching with enum
 * (which is supposed to be 'int'), that would be okay, but not as clean
 * for type-checking.
 */
typedef unsigned short LYKeymap_t;

#define KEYMAP_SIZE 661
extern LYKeymap_t keymap[KEYMAP_SIZE]; /* main keymap matrix */

#ifdef EXP_KEYBOARD_LAYOUT
typedef unsigned short LYKbLayout_t;
extern int current_layout;
extern LYKbLayout_t * LYKbLayouts[];
extern char * LYKbLayoutNames[];
extern int LYSetKbLayout PARAMS((char *layout_id));
#endif

#if defined(DIRED_SUPPORT) && defined(OK_OVERRIDE)
extern LYKeymap_t key_override[];
#endif

/* * *  LynxKeyCodes  * * */
#define LKC_ISLECLAC	0x8000	/* flag: contains lynxaction + editaction */
#define LKC_MOD1	0x4000	/* a modifier bit - currently for ^x-map */
#define LKC_MOD2	0x2000	/* another one - currently for esc-map */
#define LKC_MOD3	0x1000	/* another one - currently for double-map */
#define LKC_ISLAC	0x0800	/* flag: lynxkeycode already lynxactioncode */

/* Used to distinguish internal Lynx keycodes of (say) extended ncurses once. */
#define LKC_ISLKC	0x0400	/* flag: already lynxkeycode (not native) */
		     /* 0x0400  is MOUSE_KEYSYM for slang in LYStrings.c */
#define LKC_MASK	0x07FF	/* mask for lynxkeycode proper */

#define LKC_DONE	0x07FE	/* special value - operation done, not-a-key */

/* * *  LynxActionCodes  * * */
#define LAC_SHIFT	8	/* shift for lynxactioncode - must not
				   overwrite any assigned LYK_* values */
#define LAC_MASK	((1<<LAC_SHIFT)-1)
				/* mask for lynxactioncode - must cover all
				   assigned LYK_* values */

/*  Return lkc masking single actioncode, given an lkc masking a lac + lec */
#define LKC2_TO_LKC(c)   (((c) == -1 || !((c) & LKC_ISLECLAC)) ? (c) : \
			    (((c) & LAC_MASK) | LKC_ISLAC))

/*  Return lynxeditactioncode, given an lkc masking a lac + lec */
#define LKC2_TO_LEC(c)   (((c) == -1 || !((c) & LKC_ISLECLAC)) ? (c) : \
			    ((((c)&~LKC_ISLECLAC)>>LAC_SHIFT) & LAC_MASK))

/*  Convert lynxkeycode to lynxactioncode.  Modifiers are dropped.  */
#define LKC_TO_LAC(ktab,c) (((c) == -1) ? ktab[0] : \
			    ((c) & (LKC_ISLECLAC|LKC_ISLAC)) ? ((c) & LAC_MASK) : \
			    ktab[((c) & LKC_MASK) + 1])


/*  Mask lynxactioncode as a lynxkeycode.  */
#define LAC_TO_LKC0(a) ((a)|LKC_ISLAC)

/*  Mask a lynxactioncode and an editactioncode as a lynxkeycode.  */
#define LACLEC_TO_LKC0(a,b) ((a)|((b)<<LAC_SHIFT)|LKC_ISLECLAC)

/*  Convert lynxactioncode to a lynxkeycode, attempting reverse mapping.  */
#define LAC_TO_LKC(a) ((LYReverseKeymap(a)>=0)?LYReverseKeymap(a):LAC_TO_LKC0(a))

/*  Simplify a lynxkeycode:
    attempt reverse mapping if a single masked lynxactioncode, drop modifiers.  */
#define LKC_TO_C(c) ((c&LKC_ISLECLAC)? c : (c&LKC_ISLAC)? LAC_TO_LKC(c&LAC_MASK) : (c&LKC_MASK))

#define LKC_HAS_ESC_MOD(c) (c >= 0 && !(c&LKC_ISLECLAC) && (c&LKC_MOD2))


/* *  The defined LynxActionCodes  * */

/*  Variables for holding and passing around lynxactioncodes are
 *  generally of type int, the types LYKeymap_t and LYKeymapCodes
 *  are currently only used for the definitions.  That could change. - kw
 *
 *  The values in this enum are indexed against the command names in the
 *  'revmap[]' array in LYKeymap.c
 */
typedef enum {
    LYK_UNKNOWN=0
  , LYK_COMMAND
  , LYK_1
  , LYK_2
  , LYK_3
  , LYK_4
  , LYK_5
  , LYK_6
  , LYK_7
  , LYK_8
  , LYK_9
  , LYK_SOURCE
  , LYK_RELOAD
  , LYK_QUIT
  , LYK_ABORT
  , LYK_NEXT_PAGE
  , LYK_PREV_PAGE
  , LYK_UP_TWO
  , LYK_DOWN_TWO
  , LYK_UP_HALF
  , LYK_DOWN_HALF
  , LYK_REFRESH
  , LYK_HOME
  , LYK_END
  , LYK_FIRST_LINK
  , LYK_LAST_LINK
  , LYK_PREV_LINK
  , LYK_NEXT_LINK
  , LYK_LPOS_PREV_LINK
  , LYK_LPOS_NEXT_LINK
  , LYK_FASTBACKW_LINK
  , LYK_FASTFORW_LINK
  , LYK_UP_LINK
  , LYK_DOWN_LINK
  , LYK_RIGHT_LINK
  , LYK_LEFT_LINK
  , LYK_HISTORY
  , LYK_PREV_DOC
  , LYK_NEXT_DOC
  , LYK_ACTIVATE
  , LYK_SUBMIT	/* mostly like LYK_ACTIVATE, for mouse use, don't map */
  , LYK_GOTO
  , LYK_ECGOTO
  , LYK_HELP
  , LYK_DWIMHELP
  , LYK_INDEX
  , LYK_NOCACHE
  , LYK_INTERRUPT
  , LYK_MAIN_MENU
  , LYK_OPTIONS
  , LYK_INDEX_SEARCH
  , LYK_WHEREIS
  , LYK_PREV
  , LYK_NEXT
  , LYK_COMMENT
  , LYK_EDIT
  , LYK_INFO
  , LYK_PRINT
  , LYK_ADD_BOOKMARK
  , LYK_DEL_BOOKMARK
  , LYK_VIEW_BOOKMARK
  , LYK_VLINKS
  , LYK_SHELL
  , LYK_DOWNLOAD
  , LYK_TRACE_TOGGLE
  , LYK_TRACE_LOG
  , LYK_IMAGE_TOGGLE
  , LYK_INLINE_TOGGLE
  , LYK_HEAD
  , LYK_DO_NOTHING
  , LYK_TOGGLE_HELP
  , LYK_JUMP
  , LYK_KEYMAP
  , LYK_LIST
  , LYK_TOOLBAR
  , LYK_HISTORICAL
  , LYK_MINIMAL
  , LYK_SOFT_DQUOTES
  , LYK_RAW_TOGGLE
  , LYK_COOKIE_JAR
  , LYK_F_LINK_NUM
  , LYK_CLEAR_AUTH
  , LYK_SWITCH_DTD
  , LYK_ELGOTO
  , LYK_CHANGE_LINK
  , LYK_DWIMEDIT
  , LYK_EDIT_TEXTAREA
  , LYK_GROW_TEXTAREA
  , LYK_INSERT_FILE

#ifdef EXP_ADDRLIST_PAGE
  , LYK_ADDRLIST
#else
#define LYK_ADDRLIST      LYK_ADD_BOOKMARK
#endif

#ifdef USE_EXTERNALS
  , LYK_EXTERN_LINK
  , LYK_EXTERN_PAGE
#else
#define LYK_EXTERN_LINK   LYK_UNKNOWN
#define LYK_EXTERN_PAGE   LYK_UNKNOWN
#endif /* !defined(USE_EXTERNALS) */

#if defined(VMS) || defined(DIRED_SUPPORT)
  , LYK_DIRED_MENU
#else
#define LYK_DIRED_MENU    LYK_UNKNOWN
#endif /* VMS || DIRED_SUPPORT */

#ifdef DIRED_SUPPORT
  , LYK_CREATE
  , LYK_REMOVE
  , LYK_MODIFY
  , LYK_TAG_LINK
  , LYK_UPLOAD
  , LYK_INSTALL
#else
#define LYK_TAG_LINK      LYK_UNKNOWN
#endif /* DIRED_SUPPORT */

#ifdef SH_EX
  , LYK_CHG_CENTER
#endif /* SH_EX */

#ifdef KANJI_CODE_OVERRIDE
  , LYK_CHG_KCODE
#endif

#ifdef SUPPORT_CHDIR
  , LYK_CHDIR
#endif

#ifdef USE_CURSES_PADS
  , LYK_SHIFT_LEFT
  , LYK_SHIFT_RIGHT
  , LYK_LINEWRAP_TOGGLE
#else
#define LYK_SHIFT_LEFT      LYK_UNKNOWN
#define LYK_SHIFT_RIGHT     LYK_UNKNOWN
#define LYK_LINEWRAP_TOGGLE LYK_UNKNOWN
#endif

#ifdef CAN_CUT_AND_PASTE
  , LYK_PASTE_URL
  , LYK_TO_CLIPBOARD
#else
#define LYK_PASTE_URL      LYK_UNKNOWN
#define LYK_TO_CLIPBOARD   LYK_UNKNOWN
#endif

#ifdef EXP_NESTED_TABLES
  , LYK_NESTED_TABLES
#else
#define LYK_NESTED_TABLES  LYK_UNKNOWN
#endif

} LYKeymapCode;

/*
 * Symbol table for internal commands.
 */
typedef struct {
	LYKeymapCode code;
	CONST char *name;
	CONST char *doc;
} Kcmd;

extern Kcmd * LYKeycodeToKcmd PARAMS((LYKeymapCode code));
extern Kcmd * LYStringToKcmd PARAMS((CONST char * name));

#endif /* LYKEYMAP_H */
