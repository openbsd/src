/*	$OpenBSD: term.c,v 1.2 1996/09/21 06:23:22 downsj Exp $	*/
/* vi:set ts=4 sw=4:
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */
/*
 *
 * term.c: functions for controlling the terminal
 *
 * primitive termcap support for Amiga, MSDOS, and Win32 included
 *
 * NOTE: padding and variable substitution is not performed,
 * when compiling without HAVE_TGETENT, we use tputs() and tgoto() dummies.
 */

/*
 * Some systems have a prototype for tgetstr() with (char *) instead of
 * (char **). This define removes that prototype. We include our own prototype
 * below.
 */

#define tgetstr tgetstr_defined_wrong
#include "vim.h"

#include "globals.h"
#include "option.h"
#include "proto.h"

#ifdef HAVE_TGETENT
# ifdef HAVE_TERMCAP_H
#  include <termcap.h>
# endif

/*
 * A few linux systems define outfuntype in termcap.h to be used as the third
 * argument for tputs().
 */
# ifdef VMS
#  define TPUTSFUNCAST
# else
#  ifdef HAVE_OUTFUNTYPE
#   define TPUTSFUNCAST (outfuntype)
#  else
#   define TPUTSFUNCAST (int (*)())
#  endif
# endif
#endif

#undef tgetstr

/*
 * Here are the builtin termcap entries.  They are not stored as complete
 * Tcarr structures, as such a structure is too big.
 *
 * The entries are compact, therefore they normally are included even when
 * HAVE_TGETENT is defined.	When HAVE_TGETENT is defined, the builtin entries
 * can be accessed with "builtin_amiga", "builtin_ansi", "builtin_debug", etc.
 *
 * Each termcap is a list of builtin_term structures. It always starts with
 * KS_NAME, which separates the entries.  See parse_builtin_tcap() for all
 * details.
 * bt_entry is either a KS_xxx code (< 0x100), or a K_xxx code.
 */
struct builtin_term
{
	int			bt_entry;
	char		*bt_string;
};

/* start of keys that are not directly used by Vim but can be mapped */
#define BT_EXTRA_KEYS	0x101

static struct builtin_term *find_builtin_term __ARGS((char_u *name));
static void parse_builtin_tcap __ARGS((char_u *s));
static void gather_termleader __ARGS((void));
static int get_bytes_from_buf __ARGS((char_u *, char_u *, int));
static int is_builtin_term __ARGS((char_u *));

#ifdef HAVE_TGETENT
static char_u *tgetent_error __ARGS((char_u *, char_u *));

/*
 * Here is our own prototype for tgetstr(), any prototypes from the include
 * files have been disabled by the define at the start of this file.
 */
char			*tgetstr __PARMS((char *, char **));

/*
 * Don't declare these variables if termcap.h contains them.
 * Autoconf checks if these variables should be declared extern (not all
 * systems have them).
 * Some versions define ospeed to be speed_t, but that is incompatible with
 * BSD, where ospeed is short and speed_t is long.
 */
# ifndef HAVE_OSPEED
#  ifdef OSPEED_EXTERN
extern short ospeed;
#   else
short ospeed;
#   endif
# endif
# ifndef HAVE_UP_BC_PC
#  ifdef UP_BC_PC_EXTERN
extern char *UP, *BC, PC;
#  else
char *UP, *BC, PC;
#  endif
# endif

# define TGETSTR(s, p)	(char_u *)tgetstr((s), (char **)(p))
# define TGETENT(b, t)	tgetent((char *)(b), (char *)(t))

#endif /* HAVE_TGETENT */

struct builtin_term builtin_termcaps[] =
{

#if defined(USE_GUI)
/*
 * Motif/Athena pseudo term-cap.
 */
	{KS_NAME,		"gui"},
	{KS_CE,			"\033|$"},
	{KS_AL,			"\033|i"},
# ifdef TERMINFO
	{KS_CAL,		"\033|%p1%dI"},
# else
	{KS_CAL,		"\033|%dI"},
# endif
	{KS_DL,			"\033|d"},
# ifdef TERMINFO
	{KS_CDL,		"\033|%p1%dD"},
	{KS_CS,			"\033|%p1%d;%p2%dR"},
# else
	{KS_CDL,		"\033|%dD"},
	{KS_CS,			"\033|%d;%dR"},
# endif
	{KS_CL,			"\033|C"},
	{KS_ME,			"\033|63H"},	/* 63 = HL_ALL,			H = off */
	{KS_MR,			"\033|1h"},		/* 1  = HL_INVERSE,		h = on */
	{KS_MD,			"\033|2h"},		/* 2  = HL_BOLD,		h = on */
	{KS_SE,			"\033|16H"},	/* 16 = HL_STANDOUT,	H = off */
	{KS_SO,			"\033|16h"},	/* 16 = HL_STANDOUT,	h = on */
	{KS_UE,			"\033|8H"},		/* 8  = HL_UNDERLINE,	H = off */
	{KS_US,			"\033|8h"},		/* 8  = HL_UNDERLINE,	h = on */
	{KS_CZR,		"\033|4H"},		/* 4  = HL_ITAL,		H = off */
	{KS_CZH,		"\033|4h"},		/* 4  = HL_ITAL,		h = on */
	{KS_VB,			"\033|f"},
# ifdef TERMINFO
	{KS_CM,			"\033|%p1%d;%p2%dM"},
# else
	{KS_CM,			"\033|%d;%dM"},
# endif
		/* there are no key sequences here, the GUI sequences are recognized
		 * in check_termcodes() */
#endif

#ifndef NO_BUILTIN_TCAPS

# if defined(AMIGA) || defined(ALL_BUILTIN_TCAPS)
/*
 * Amiga console window, default for Amiga
 */
	{KS_NAME,		"amiga"},
	{KS_CE,			"\033[K"},
	{KS_CD,			"\033[J"},
	{KS_AL,			"\033[L"},
#  ifdef TERMINFO
	{KS_CAL,		"\033[%p1%dL"},
#  else
	{KS_CAL,		"\033[%dL"},
#  endif
	{KS_DL,			"\033[M"},
#  ifdef TERMINFO
	{KS_CDL,		"\033[%p1%dM"},
#  else
	{KS_CDL,		"\033[%dM"},
#  endif
	{KS_CL,			"\014"},
	{KS_VI,			"\033[0 p"},
	{KS_VE,			"\033[1 p"},
	{KS_ME,			"\033[0m"},
	{KS_MR,			"\033[7m"},
	{KS_MD,			"\033[1m"},
	{KS_SE,			"\033[0m"},
	{KS_SO,			"\033[33m"},
	{KS_US,			"\033[4m"},
	{KS_UE,			"\033[0m"},
	{KS_CZH,		"\033[3m"},
	{KS_CZR,		"\033[0m"},
	{KS_MS,			"\001"},
#  ifdef TERMINFO
	{KS_CM,			"\033[%i%p1%d;%p2%dH"},
#  else
	{KS_CM,			"\033[%i%d;%dH"},
#  endif
#  ifdef TERMINFO
	{KS_CRI,		"\033[%p1%dC"},
#  else
	{KS_CRI,		"\033[%dC"},
#  endif
	{K_UP,			"\233A"},
	{K_DOWN,		"\233B"},
	{K_LEFT,		"\233D"},
	{K_RIGHT,		"\233C"},
	{K_S_UP,		"\233T"},
	{K_S_DOWN,		"\233S"},
	{K_S_LEFT,		"\233 A"},
	{K_S_RIGHT,		"\233 @"},
	{K_S_TAB,		"\233Z"},
	{K_F1,			"\233\060~"},/* some compilers don't understand "\2330" */
	{K_F2,			"\233\061~"},
	{K_F3,			"\233\062~"},
	{K_F4,			"\233\063~"},
	{K_F5,			"\233\064~"},
	{K_F6,			"\233\065~"},
	{K_F7,			"\233\066~"},
	{K_F8,			"\233\067~"},
	{K_F9,			"\233\070~"},
	{K_F10,			"\233\071~"},
	{K_S_F1,		"\233\061\060~"},
	{K_S_F2,		"\233\061\061~"},
	{K_S_F3,		"\233\061\062~"},
	{K_S_F4,		"\233\061\063~"},
	{K_S_F5,		"\233\061\064~"},
	{K_S_F6,		"\233\061\065~"},
	{K_S_F7,		"\233\061\066~"},
	{K_S_F8,		"\233\061\067~"},
	{K_S_F9,		"\233\061\070~"},
	{K_S_F10,		"\233\061\071~"},
	{K_HELP,		"\233?~"},
	{K_INS,			"\233\064\060~"},	/* 101 key keyboard */
	{K_PAGEUP,		"\233\064\061~"},	/* 101 key keyboard */
	{K_PAGEDOWN,	"\233\064\062~"},	/* 101 key keyboard */
	{K_HOME,		"\233\064\064~"},	/* 101 key keyboard */
	{K_END,			"\233\064\065~"},	/* 101 key keyboard */

	{BT_EXTRA_KEYS,	""},
	{TERMCAP2KEY('#', '2'),	"\233\065\064~"},	/* shifted home key */
	{TERMCAP2KEY('#', '3'),	"\233\065\060~"},	/* shifted insert key */
	{TERMCAP2KEY('*', '7'),	"\233\065\065~"},	/* shifted end key */
# endif

# if defined(UNIX) || defined(ALL_BUILTIN_TCAPS) || defined(SOME_BUILTIN_TCAPS) || defined(__EMX__)
/*
 * standard ANSI terminal, default for unix
 */
	{KS_NAME,		"ansi"},
	{KS_CE,			"\033[K"},
	{KS_AL,			"\033[L"},
#  ifdef TERMINFO
	{KS_CAL,		"\033[%p1%dL"},
#  else
	{KS_CAL,		"\033[%dL"},
#  endif
	{KS_DL,			"\033[M"},
#  ifdef TERMINFO
	{KS_CDL,		"\033[%p1%dM"},
#  else
	{KS_CDL,		"\033[%dM"},
#  endif
	{KS_CL,			"\033[H\033[2J"},
	{KS_ME,			"\033[0m"},
	{KS_MR,			"\033[7m"},
	{KS_MS,			"\001"},
#  ifdef TERMINFO
	{KS_CM,			"\033[%i%p1%d;%p2%dH"},
#  else
	{KS_CM,			"\033[%i%d;%dH"},
#  endif
#  ifdef TERMINFO
	{KS_CRI,		"\033[%p1%dC"},
#  else
	{KS_CRI,		"\033[%dC"},
#  endif
# endif

# if defined(MSDOS) || defined(ALL_BUILTIN_TCAPS) || defined(__EMX__)
/*
 * These codes are valid when nansi.sys or equivalent has been installed.
 * Function keys on a PC are preceded with a NUL. These are converted into
 * K_NUL '\316' in mch_inchar(), because we cannot handle NULs in key codes.
 * CTRL-arrow is used instead of SHIFT-arrow.
 */
#ifdef __EMX__
	{KS_NAME,		"os2ansi"},
#else
	{KS_NAME,		"pcansi"},
	{KS_DL,			"\033[M"},
	{KS_AL,			"\033[L"},
#endif
	{KS_CE,			"\033[K"},
	{KS_CL,			"\033[2J"},
	{KS_ME,			"\033[0m"},
	{KS_MR,			"\033[7m"},
	{KS_MS,			"\001"},
#  ifdef TERMINFO
	{KS_CM,			"\033[%i%p1%d;%p2%dH"},
#  else
	{KS_CM,			"\033[%i%d;%dH"},
#  endif
#  ifdef TERMINFO
	{KS_CRI,		"\033[%p1%dC"},
#  else
	{KS_CRI,		"\033[%dC"},
#  endif
	{K_UP,			"\316H"},
	{K_DOWN,		"\316P"},
	{K_LEFT,		"\316K"},
	{K_RIGHT,		"\316M"},
	{K_S_LEFT,		"\316s"},
	{K_S_RIGHT,		"\316t"},
	{K_F1,			"\316;"},
	{K_F2,			"\316<"},
	{K_F3,			"\316="},
	{K_F4,			"\316>"},
	{K_F5,			"\316?"},
	{K_F6,			"\316@"},
	{K_F7,			"\316A"},
	{K_F8,			"\316B"},
	{K_F9,			"\316C"},
	{K_F10,			"\316D"},
	{K_F11,			"\316\205"},	/* guessed */
	{K_F12,			"\316\206"},	/* guessed */
	{K_S_F1,		"\316T"},
	{K_S_F2,		"\316U"},
	{K_S_F3,		"\316V"},
	{K_S_F4,		"\316W"},
	{K_S_F5,		"\316X"},
	{K_S_F6,		"\316Y"},
	{K_S_F7,		"\316Z"},
	{K_S_F8,		"\316["},
	{K_S_F9,		"\316\\"},
	{K_S_F10,		"\316]"},
	{K_S_F11,		"\316\207"},	/* guessed */
	{K_S_F12,		"\316\210"},	/* guessed */
	{K_INS,			"\316R"},
	{K_DEL,			"\316S"},
	{K_HOME,		"\316G"},
	{K_END,			"\316O"},
	{K_PAGEDOWN,	"\316Q"},
	{K_PAGEUP,		"\316I"},
# endif

# if defined(MSDOS)
/*
 * These codes are valid for the pc video.	The entries that start with ESC |
 * are translated into conio calls in msdos.c. Default for MSDOS.
 */
	{KS_NAME,		"pcterm"},
	{KS_CE,			"\033|K"},
	{KS_AL,			"\033|L"},
	{KS_DL,			"\033|M"},
#  ifdef TERMINFO
	{KS_CS,			"\033|%i%p1%d;%p2%dr"},
#  else
	{KS_CS,			"\033|%i%d;%dr"},
#  endif
	{KS_CL,			"\033|J"},
	{KS_ME,			"\033|0m"},
	{KS_MR,			"\033|112m"},
	{KS_MD,			"\033|63m"},
	{KS_SE,			"\033|0m"},
	{KS_SO,			"\033|31m"},
	{KS_CZH,		"\033|225m"},	/* italic mode: blue text on yellow */
	{KS_CZR,		"\033|0m"},		/* italic mode end */
	{KS_US,			"\033|67m"},	/* underscore mode: cyan text on red */
	{KS_UE,			"\033|0m"},		/* underscore mode end */
	{KS_MS,			"\001"},
#  ifdef TERMINFO
	{KS_CM,			"\033|%i%p1%d;%p2%dH"},
#  else
	{KS_CM,			"\033|%i%d;%dH"},
#  endif
	{K_UP,			"\316H"},
	{K_DOWN,		"\316P"},
	{K_LEFT,		"\316K"},
	{K_RIGHT,		"\316M"},
	{K_S_LEFT,		"\316s"},
	{K_S_RIGHT,		"\316t"},
	{K_S_TAB,		"\316\017"},
	{K_F1,			"\316;"},
	{K_F2,			"\316<"},
	{K_F3,			"\316="},
	{K_F4,			"\316>"},
	{K_F5,			"\316?"},
	{K_F6,			"\316@"},
	{K_F7,			"\316A"},
	{K_F8,			"\316B"},
	{K_F9,			"\316C"},
	{K_F10,			"\316D"},
	{K_F11,			"\316\205"},	/* only when nobioskey */
	{K_F12,			"\316\206"},	/* only when nobioskey */
	{K_S_F1,		"\316T"},
	{K_S_F2,		"\316U"},
	{K_S_F3,		"\316V"},
	{K_S_F4,		"\316W"},
	{K_S_F5,		"\316X"},
	{K_S_F6,		"\316Y"},
	{K_S_F7,		"\316Z"},
	{K_S_F8,		"\316["},
	{K_S_F9,		"\316\\"},
	{K_S_F10,		"\316]"},
	{K_S_F11,		"\316\207"},	/* only when nobioskey */
	{K_S_F12,		"\316\210"},	/* only when nobioskey */
	{K_INS,			"\316R"},
	{K_DEL,			"\316S"},
	{K_HOME,		"\316G"},
	{K_END,			"\316O"},
	{K_PAGEDOWN,	"\316Q"},
	{K_PAGEUP,		"\316I"},
# endif

# if defined(WIN32) || defined(ALL_BUILTIN_TCAPS) || defined(__EMX__)
/*
 * These codes are valid for the Win32 Console .  The entries that start with
 * ESC | are translated into console calls in win32.c.  The function keys
 * are also translated in win32.c.
 */
	{KS_NAME,		"win32"},
	{KS_CE,			"\033|K"},		/* clear to end of line */
	{KS_AL,			"\033|L"},		/* add new blank line */
#  ifdef TERMINFO
	{KS_CAL,		"\033|%p1%dL"},	/* add number of new blank lines */
#  else
	{KS_CAL,		"\033|%dL"},	/* add number of new blank lines */
#  endif
	{KS_DL,			"\033|M"},		/* delete line */
#  ifdef TERMINFO
	{KS_CDL,		"\033|%p1%dM"},	/* delete number of lines */
#  else
	{KS_CDL,		"\033|%dM"},	/* delete number of lines */
#  endif
	{KS_CL,			"\033|J"},		/* clear screen */
	{KS_CD,			"\033|j"},		/* clear to end of display */
	{KS_VI,			"\033|v"},		/* cursor invisible */
	{KS_VE,			"\033|V"},		/* cursor visible */

	{KS_ME,			"\033|0m"},		/* normal mode */
	{KS_MR,			"\033|112m"},	/* reverse mode: black text on lightgray */
	{KS_MD,			"\033|63m"},	/* bold mode: white text on cyan */
#if 1
	{KS_SO,			"\033|31m"},	/* standout mode: white text on blue */
	{KS_SE,			"\033|0m"},		/* standout mode end */
#else
	{KS_SO,			"\033|F"},		/* standout mode: high intensity text */
	{KS_SE,			"\033|f"},		/* standout mode end */
#endif
	{KS_CZH,		"\033|225m"},	/* italic mode: blue text on yellow */
	{KS_CZR,		"\033|0m"},		/* italic mode end */
	{KS_US,			"\033|67m"},	/* underscore mode: cyan text on red */
	{KS_UE,			"\033|0m"},		/* underscore mode end */

	{KS_MS,			"\001"},		/* save to move cur in reverse mode */
#  ifdef TERMINFO
	{KS_CM,			"\033|%i%p1%d;%p2%dH"},	/* cursor motion */
#  else
	{KS_CM,			"\033|%i%d;%dH"},		/* cursor motion */
#  endif
	{KS_VB,			"\033|B"},		/* visual bell */
	{KS_TI,			"\033|S"},		/* put terminal in termcap mode */
	{KS_TE,			"\033|E"},		/* out of termcap mode */
	{KS_CS,         "\033|%i%d;%dr"},	/* scroll region */

	{K_UP,			"\316H"},
	{K_DOWN,		"\316P"},
	{K_LEFT,		"\316K"},
	{K_RIGHT,		"\316M"},
	{K_S_UP,		"\316\304"},
	{K_S_DOWN,		"\316\317"},
	{K_S_LEFT,		"\316\311"},
	{K_S_RIGHT,		"\316\313"},
	{K_S_TAB,		"\316\017"},
	{K_F1,			"\316;"},
	{K_F2,			"\316<"},
	{K_F3,			"\316="},
	{K_F4,			"\316>"},
	{K_F5,			"\316?"},
	{K_F6,			"\316@"},
	{K_F7,			"\316A"},
	{K_F8,			"\316B"},
	{K_F9,			"\316C"},
	{K_F10,			"\316D"},
	{K_F11,			"\316\205"},
	{K_F12,			"\316\206"},
	{K_S_F1,		"\316T"},
	{K_S_F2,		"\316U"},
	{K_S_F3,		"\316V"},
	{K_S_F4,		"\316W"},
	{K_S_F5,		"\316X"},
	{K_S_F6,		"\316Y"},
	{K_S_F7,		"\316Z"},
	{K_S_F8,		"\316["},
	{K_S_F9,		"\316\\"},
	{K_S_F10,		"\316]"},
	{K_S_F11,		"\316\207"},
	{K_S_F12,		"\316\210"},
	{K_INS,			"\316R"},
	{K_DEL,			"\316S"},
	{K_HOME,		"\316G"},
	{K_END,			"\316O"},
	{K_PAGEDOWN,	"\316Q"},
	{K_PAGEUP,		"\316I"},
# endif

# if defined(ALL_BUILTIN_TCAPS) || defined(MINT)
/*
 * Ordinary vt52
 */
	{KS_NAME,	 	"vt52"},
	{KS_CE,			"\033K"},
	{KS_CD,			"\033J"},
	{KS_CM,			"\033Y%+ %+ "},
#  ifdef MINT
	{KS_AL,			"\033L"},
	{KS_DL,			"\033M"},
	{KS_CL,			"\033E"},
	{KS_SR,			"\033I"},
	{KS_VE,			"\033e"},
	{KS_VI,			"\033f"},
	{KS_SO,			"\033p"},
	{KS_SE,			"\033q"},
	{K_UP,			"\033A"},
	{K_DOWN,		"\033B"},
	{K_LEFT,		"\033D"},
	{K_RIGHT,		"\033C"},
	{K_S_UP,		"\033a"},
	{K_S_DOWN,		"\033b"},
	{K_S_LEFT,		"\033d"},
	{K_S_RIGHT,		"\033c"},
	{K_F1,			"\033P"},
	{K_F2,			"\033Q"},
	{K_F3,			"\033R"},
	{K_F4,			"\033S"},
	{K_F5,			"\033T"},
	{K_F6,			"\033U"},
	{K_F7,			"\033V"},
	{K_F8,			"\033W"},
	{K_F9,			"\033X"},
	{K_F10,			"\033Y"},
	{K_S_F1,		"\033p"},
	{K_S_F2,		"\033q"},
	{K_S_F3,		"\033r"},
	{K_S_F4,		"\033s"},
	{K_S_F5,		"\033t"},
	{K_S_F6,		"\033u"},
	{K_S_F7,		"\033v"},
	{K_S_F8,		"\033w"},
	{K_S_F9,		"\033x"},
	{K_S_F10,		"\033y"},
	{K_INS,			"\033I"},
	{K_HOME,		"\033E"},
	{K_PAGEDOWN,	"\033b"},
	{K_PAGEUP,		"\033a"},
#  else
	{KS_AL,			"\033T"},
	{KS_DL,			"\033U"},
	{KS_CL,			"\033H\033J"},
	{KS_ME,			"\033SO"},
	{KS_MR,			"\033S2"},
	{KS_MS,			"\001"},
#  endif
# endif

# if defined(UNIX) || defined(ALL_BUILTIN_TCAPS) || defined(SOME_BUILTIN_TCAPS) || defined(__EMX__)
/*
 * The xterm termcap is missing F14 and F15, because they send the same
 * codes as the undo and help key, although they don't work on all keyboards.
 */
	{KS_NAME,		"xterm"},
	{KS_CE,			"\033[K"},
	{KS_AL,			"\033[L"},
#  ifdef TERMINFO
	{KS_CAL,		"\033[%p1%dL"},
#  else
	{KS_CAL,		"\033[%dL"},
#  endif
	{KS_DL,			"\033[M"},
#  ifdef TERMINFO
	{KS_CDL,		"\033[%p1%dM"},
#  else
	{KS_CDL,		"\033[%dM"},
#  endif
#  ifdef TERMINFO
	{KS_CS,			"\033[%i%p1%d;%p2%dr"},
#  else
	{KS_CS,			"\033[%i%d;%dr"},
#  endif
	{KS_CL,			"\033[H\033[2J"},
	{KS_CD,			"\033[J"},
	{KS_ME,			"\033[m"},
	{KS_MR,			"\033[7m"},
	{KS_MD,			"\033[1m"},
	{KS_UE,			"\033[m"},
	{KS_US,			"\033[4m"},
	{KS_MS,			"\001"},
#  ifdef TERMINFO
	{KS_CM,			"\033[%i%p1%d;%p2%dH"},
#  else
	{KS_CM,			"\033[%i%d;%dH"},
#  endif
	{KS_SR,			"\033M"},
#  ifdef TERMINFO
	{KS_CRI,		"\033[%p1%dC"},
#  else
	{KS_CRI,		"\033[%dC"},
#  endif
	{KS_KS,			"\033[?1h\033="},
	{KS_KE,			"\033[?1l\033>"},
#  ifdef SAVE_XTERM_SCREEN
	{KS_TI,			"\0337\033[?47h"},
	{KS_TE,			"\033[2J\033[?47l\0338"},
#  endif
	{K_UP,			"\033OA"},
	{K_DOWN,		"\033OB"},
	{K_LEFT,		"\033OD"},
	{K_RIGHT,		"\033OC"},
	{K_S_UP,		"\033Ox"},
	{K_S_DOWN,		"\033Or"},
	{K_S_LEFT,		"\033Ot"},
	{K_S_RIGHT,		"\033Ov"},
	{K_F1,			"\033[11~"},
	{K_F2,			"\033[12~"},
	{K_F3,			"\033[13~"},
	{K_F4,			"\033[14~"},
	{K_F5,			"\033[15~"},
	{K_F6,			"\033[17~"},
	{K_F7,			"\033[18~"},
	{K_F8,			"\033[19~"},
	{K_F9,			"\033[20~"},
	{K_F10,			"\033[21~"},
	{K_F11,			"\033[23~"},
	{K_F12,			"\033[24~"},
	{K_HELP,		"\033[28~"},
	{K_UNDO,		"\033[26~"},
	{K_INS,			"\033[2~"},
	{K_HOME,		"\033[7~"},
	{K_KHOME,		"\033[1~"},
	{K_END,			"\033[8~"},
	{K_KEND,		"\033[4~"},
	{K_PAGEUP,		"\033[5~"},
	{K_PAGEDOWN,	"\033[6~"},
	/* {K_DEL,			"\033[3~"}, not used */

	{BT_EXTRA_KEYS,	""},
	{TERMCAP2KEY('k', '0'),	"\033[10~"},	/* F0 */
	{TERMCAP2KEY('F', '3'),	"\033[25~"},	/* F13 */
	{TERMCAP2KEY('F', '6'),	"\033[29~"},	/* F16 */
	{TERMCAP2KEY('F', '7'),	"\033[31~"},	/* F17 */
	{TERMCAP2KEY('F', '8'),	"\033[32~"},	/* F18 */
	{TERMCAP2KEY('F', '9'),	"\033[33~"},	/* F19 */
	{TERMCAP2KEY('F', 'A'),	"\033[34~"},	/* F20 */
# endif

# if defined(UNIX) || defined(ALL_BUILTIN_TCAPS)
/*
 * iris-ansi for Silicon Graphics machines.
 */
	{KS_NAME,		"iris-ansi"},
	{KS_CE,			"\033[K"},
	{KS_CD,			"\033[J"},
	{KS_AL,			"\033[L"},
#  ifdef TERMINFO
	{KS_CAL,		"\033[%p1%dL"},
#  else
	{KS_CAL,		"\033[%dL"},
#  endif
	{KS_DL,			"\033[M"},
#  ifdef TERMINFO
	{KS_CDL,		"\033[%p1%dM"},
#  else
	{KS_CDL,		"\033[%dM"},
#  endif
/*
 * This "cs" is not working correctly. What is the right one?
 */
#if 0
#  ifdef TERMINFO
	{KS_CS,			"\033[%i%p1%d;%p2%dr"},
#  else
	 {KS_CS,		"\033[%i%d;%dr"},
#  endif
#endif
	{KS_CL,			"\033[H\033[2J"},
	{KS_VE,			"\033[9/y\033[12/y\033[=6l"},
	{KS_VS,			"\033[10/y\033[=1h\033[=2l\033[=6h"},
	{KS_SE,			"\033[m"},
	{KS_SO,			"\033[1;7m"},
	{KS_ME,			"\033[m"},
	{KS_MR,			"\033[7m"},
	{KS_MD,			"\033[1m"},
	{KS_UE,			"\033[m"},
	{KS_US,			"\033[4m"},
	{KS_MS,			"\001"}, /* does this really work? */
#  ifdef TERMINFO
	{KS_CM,			"\033[%i%p1%d;%p2%dH"},
#  else
	{KS_CM,			"\033[%i%d;%dH"},
#  endif
	{KS_SR,			"\033M"},
#  ifdef TERMINFO
	{KS_CRI,		"\033[%p1%dC"},
#  else
	{KS_CRI,		"\033[%dC"},
#  endif
	{K_UP,			"\033[A"},
	{K_DOWN,		"\033[B"},
	{K_LEFT,		"\033[D"},
	{K_RIGHT,		"\033[C"},
	{K_S_UP,		"\033[161q"},
	{K_S_DOWN,		"\033[164q"},
	{K_S_LEFT,		"\033[158q"},
	{K_S_RIGHT,		"\033[167q"},
	{K_F1,			"\033[001q"},
	{K_F2,			"\033[002q"},
	{K_F3,			"\033[003q"},
	{K_F4,			"\033[004q"},
	{K_F5,			"\033[005q"},
	{K_F6,			"\033[006q"},
	{K_F7,			"\033[007q"},
	{K_F8,			"\033[008q"},
	{K_F9,			"\033[009q"},
	{K_F10,			"\033[010q"},
	{K_F11,			"\033[011q"},
	{K_F12,			"\033[012q"},
	{K_S_F1,		"\033[013q"},
	{K_S_F2,		"\033[014q"},
	{K_S_F3,		"\033[015q"},
	{K_S_F4,		"\033[016q"},
	{K_S_F5,		"\033[017q"},
	{K_S_F6,		"\033[018q"},
	{K_S_F7,		"\033[019q"},
	{K_S_F8,		"\033[020q"},
	{K_S_F9,		"\033[021q"},
	{K_S_F10,		"\033[022q"},
	{K_S_F11,		"\033[023q"},
	{K_S_F12,		"\033[024q"},
	{K_INS,			"\033[139q"},
	{K_HOME,		"\033[H"},
	{K_END,			"\033[146q"},
	{K_PAGEUP,		"\033[150q"},
	{K_PAGEDOWN,	"\033[154q"},
# endif

# if defined(DEBUG) || defined(ALL_BUILTIN_TCAPS)
/*
 * for debugging
 */
	{KS_NAME,		"debug"},
	{KS_CE,			"[CE]"},
	{KS_CD,			"[CD]"},
	{KS_AL,			"[AL]"},
#  ifdef TERMINFO
	{KS_CAL,		"[CAL%p1%d]"},
#  else
	{KS_CAL,		"[CAL%d]"},
#  endif
	{KS_DL,			"[DL]"},
#  ifdef TERMINFO
	{KS_CDL,		"[CDL%p1%d]"},
#  else
	{KS_CDL,		"[CDL%d]"},
#  endif
#  ifdef TERMINFO
	{KS_CS,			"[%dCS%p1%d]"},
#  else
	{KS_CS,			"[%dCS%d]"},
#  endif
	{KS_CL,			"[CL]"},
	{KS_VI,			"[VI]"},
	{KS_VE,			"[VE]"},
	{KS_VS,			"[VS]"},
	{KS_ME,			"[ME]"},
	{KS_MR,			"[MR]"},
	{KS_MD,			"[MD]"},
	{KS_SE,			"[SE]"},
	{KS_SO,			"[SO]"},
	{KS_UE,			"[UE]"},
	{KS_US,			"[US]"},
	{KS_MS,			"[MS]"},
#  ifdef TERMINFO
	{KS_CM,			"[%p1%dCM%p2%d]"},
#  else
	{KS_CM,			"[%dCM%d]"},
#  endif
	{KS_SR,			"[SR]"},
#  ifdef TERMINFO
	{KS_CRI,		"[CRI%p1%d]"},
#  else
	{KS_CRI,		"[CRI%d]"},
#  endif
	{KS_VB,			"[VB]"},
	{KS_KS,			"[KS]"},
	{KS_KE,			"[KE]"},
	{KS_TI,			"[TI]"},
	{KS_TE,			"[TE]"},
	{K_UP,			"[KU]"},
	{K_DOWN,		"[KD]"},
	{K_LEFT,		"[KL]"},
	{K_RIGHT,		"[KR]"},
	{K_S_UP,		"[S-KU]"},
	{K_S_DOWN,		"[S-KD]"},
	{K_S_LEFT,		"[S-KL]"},
	{K_S_RIGHT,		"[S-KR]"},
	{K_F1,			"[F1]"},
	{K_F2,			"[F2]"},
	{K_F3,			"[F3]"},
	{K_F4,			"[F4]"},
	{K_F5,			"[F5]"},
	{K_F6,			"[F6]"},
	{K_F7,			"[F7]"},
	{K_F8,			"[F8]"},
	{K_F9,			"[F9]"},
	{K_F10,			"[F10]"},
	{K_F11,			"[F11]"},
	{K_F12,			"[F12]"},
	{K_S_F1,		"[S-F1]"},
	{K_S_F2,		"[S-F2]"},
	{K_S_F3,		"[S-F3]"},
	{K_S_F4,		"[S-F4]"},
	{K_S_F5,		"[S-F5]"},
	{K_S_F6,		"[S-F6]"},
	{K_S_F7,		"[S-F7]"},
	{K_S_F8,		"[S-F8]"},
	{K_S_F9,		"[S-F9]"},
	{K_S_F10,		"[S-F10]"},
	{K_S_F11,		"[S-F11]"},
	{K_S_F12,		"[S-F12]"},
	{K_HELP,		"[HELP]"},
	{K_UNDO,		"[UNDO]"},
	{K_BS,			"[BS]"},
	{K_INS,			"[INS]"},
	{K_DEL,			"[DEL]"},
	{K_HOME,		"[HOME]"},
	{K_END,			"[END]"},
	{K_PAGEUP,		"[PAGEUP]"},
	{K_PAGEDOWN,	"[PAGEDOWN]"},
	{K_KHOME,		"[KHOME]"},
	{K_KEND,		"[KEND]"},
	{K_KPAGEUP,		"[KPAGEUP]"},
	{K_KPAGEDOWN,	"[KPAGEDOWN]"},
	{K_MOUSE,		"[MOUSE]"},
# endif

#endif /* NO_BUILTIN_TCAPS */

/*
 * The most minimal terminal: only clear screen and cursor positioning
 * Always included.
 */
	{KS_NAME,		"dumb"},
	{KS_CL,			"\014"},
#ifdef TERMINFO
	{KS_CM,			"\033[%i%p1%d;%p2%dH"},
#else
	{KS_CM,			"\033[%i%d;%dH"},
#endif

/*
 * end marker
 */
	{KS_NAME,		NULL}

};		/* end of builtin_termcaps */

/*
 * DEFAULT_TERM is used, when no terminal is specified with -T option or $TERM.
 */
#ifdef AMIGA
# define DEFAULT_TERM	(char_u *)"amiga"
#endif /* AMIGA */

#ifdef WIN32
# define DEFAULT_TERM	(char_u *)"win32"
#endif /* WIN32 */
  
#ifdef MSDOS
# define DEFAULT_TERM	(char_u *)"pcterm"
#endif /* MSDOS */

#if defined(UNIX) && !defined(MINT)
# define DEFAULT_TERM	(char_u *)"ansi"
#endif /* UNIX */

#ifdef MINT
# define DEFAULT_TERM	(char_u *)"vt52"
#endif /* MINT */

#ifdef __EMX__
# define DEFAULT_TERM	(char_u *)"os2ansi"
#endif /* __EMX__ */

#ifdef VMS
# define DEFAULT_TERM	(char_u *)"ansi"
#endif /* VMS */

/*
 * Term_strings contains currently used terminal output strings.
 * It is initialized with the default values by parse_builtin_tcap().
 * The values can be changed by setting the option with the same name.
 */
char_u *(term_strings[KS_LAST + 1]);

static int		need_gather = FALSE;			/* need to fill termleader[] */
static char_u	termleader[256 + 1];			/* for check_termcode() */

	static struct builtin_term *
find_builtin_term(term)
	char_u		*term;
{
	struct builtin_term *p;

	p = builtin_termcaps;
	while (p->bt_string != NULL)
	{
		if (p->bt_entry == KS_NAME)
		{
#ifdef UNIX
			if (STRCMP(p->bt_string, "iris-ansi") == 0 && is_iris_ansi(term))
				return p;
			else if (STRCMP(p->bt_string, "xterm") == 0 && is_xterm(term))
				return p;
			else
#endif
				if (STRCMP(term, p->bt_string) == 0)
					return p;
		}
		++p;
	}
	return p;
}

/*
 * Parsing of the builtin termcap entries.
 * Caller should check if 'name' is a valid builtin term.
 * The terminal's name is not set, as this is already done in termcapinit().
 */
	static void
parse_builtin_tcap(term)
	char_u	*term;
{
	struct builtin_term		*p;
	char_u					name[2];

	p = find_builtin_term(term);
	for (++p; p->bt_entry != KS_NAME && p->bt_entry != BT_EXTRA_KEYS; ++p)
	{
		if (p->bt_entry < 0x100)	/* KS_xx entry */
		{
			if (term_strings[p->bt_entry] == NULL ||
									term_strings[p->bt_entry] == empty_option)
				term_strings[p->bt_entry] = (char_u *)p->bt_string;
		}
		else
		{
			name[0] = KEY2TERMCAP0(p->bt_entry);
			name[1] = KEY2TERMCAP1(p->bt_entry);
			if (find_termcode(name) == NULL)
				add_termcode(name, (char_u *)p->bt_string);
		}
	}
}

/*
 * Set terminal options for terminal "term".
 * Return OK if terminal 'term' was found in a termcap, FAIL otherwise.
 *
 * While doing this, until ttest(), some options may be NULL, be careful.
 */
	int
set_termname(term)
	char_u *term;
{
	struct builtin_term *termp;
#ifdef HAVE_TGETENT
	int			builtin_first = p_tbi;
	int			try;
	int			termcap_cleared = FALSE;
#endif
	int			width = 0, height = 0;
	char_u		*error_msg = NULL;
	char_u		*bs_p, *del_p;

	if (is_builtin_term(term))
	{
		term += 8;
#ifdef HAVE_TGETENT
		builtin_first = 1;
#endif
	}

/*
 * If HAVE_TGETENT is not defined, only the builtin termcap is used, otherwise:
 *   If builtin_first is TRUE:
 *     0. try builtin termcap
 *     1. try external termcap
 *     2. if both fail default to a builtin terminal
 *   If builtin_first is FALSE:
 *     1. try external termcap
 *     2. try builtin termcap, if both fail default to a builtin terminal
 */
#ifdef HAVE_TGETENT
	for (try = builtin_first ? 0 : 1; try < 3; ++try)
	{
		/*
		 * Use external termcap
		 */
		if (try == 1)
		{
			char_u			*p;
			static char_u	tstrbuf[TBUFSZ];
			int				i;
			char_u			tbuf[TBUFSZ];
			char_u			*tp = tstrbuf;
			static char 	*(key_names[]) =
							{
							"ku", "kd", "kr",	/* "kl" is a special case */
# ifdef ARCHIE
							"su", "sd",			/* Termcap code made up! */
# endif
							"#4", "%i",
							"k1", "k2", "k3", "k4", "k5", "k6",
							"k7", "k8", "k9", "k;", "F1", "F2",
							"%1", "&8", "kb", "kI", "kD", "kh", 
							"@7", "kP", "kN", "K1", "K3", "K4", "K5",
							NULL
							};
			static struct {
							int dest;		/* index in term_strings[] */
							char *name;		/* termcap name for string */
						  } string_names[] =
							{	{KS_CE, "ce"}, {KS_AL, "al"}, {KS_CAL, "AL"},
								{KS_DL, "dl"}, {KS_CDL, "DL"}, {KS_CS, "cs"},
								{KS_CL, "cl"}, {KS_CD, "cd"},
								{KS_VI, "vi"}, {KS_VE, "ve"},
								{KS_VS, "vs"}, {KS_ME, "me"}, {KS_MR, "mr"},
								{KS_MD, "md"}, {KS_SE, "se"}, {KS_SO, "so"},
								{KS_CZH, "ZH"}, {KS_CZR, "ZR"}, {KS_UE, "ue"},
								{KS_US, "us"}, {KS_CM, "cm"}, {KS_SR, "sr"},
								{KS_CRI, "RI"}, {KS_VB, "vb"}, {KS_KS, "ks"},
								{KS_KE, "ke"}, {KS_TI, "ti"}, {KS_TE, "te"},
								{0, NULL}
							};

			/*
			 * If the external termcap does not have a matching entry, try the
			 * builtin ones.
			 */
			if ((error_msg = tgetent_error(tbuf, term)) == NULL)
			{
				if (!termcap_cleared)
				{
					clear_termoptions();		/* clear old options */
					termcap_cleared = TRUE;
				}

			/* get output strings */
				for (i = 0; string_names[i].name != NULL; ++i)
				{
					if (term_strings[string_names[i].dest] == NULL ||
						   term_strings[string_names[i].dest] == empty_option)
						term_strings[string_names[i].dest] =
										   TGETSTR(string_names[i].name, &tp);
				}

				if ((T_MS == NULL || T_MS == empty_option) && tgetflag("ms"))
					T_MS = (char_u *)"yes";
				if ((T_DB == NULL || T_DB == empty_option) && tgetflag("db"))
					T_DB = (char_u *)"yes";
				if ((T_DA == NULL || T_DA == empty_option) && tgetflag("da"))
					T_DA = (char_u *)"yes";


			/* get key codes */

				for (i = 0; key_names[i] != NULL; ++i)
				{
					if (find_termcode((char_u *)key_names[i]) == NULL)
						add_termcode((char_u *)key_names[i],
												  TGETSTR(key_names[i], &tp));
				}

					/* if cursor-left == backspace, ignore it (televideo 925) */
				if (find_termcode((char_u *)"kl") == NULL)
				{
					p = TGETSTR("kl", &tp);
					if (p != NULL && *p != Ctrl('H'))
						add_termcode((char_u *)"kl", p);
				}

				if (height == 0)
					height = tgetnum("li");
				if (width == 0)
					width = tgetnum("co");

# ifndef hpux
				BC = (char *)TGETSTR("bc", &tp);
				UP = (char *)TGETSTR("up", &tp);
				p = TGETSTR("pc", &tp);
				if (p)
					PC = *p;
# endif /* hpux */
			}
		}
		else		/* try == 0 || try == 2 */
#endif /* HAVE_TGETENT */
		/*
		 * Use builtin termcap
		 */
		{
#ifdef HAVE_TGETENT
			/*
			 * If builtin termcap was already used, there is no need to search
			 * for the builtin termcap again, quit now.
			 */
			if (try == 2 && builtin_first && termcap_cleared)
				break;
#endif
			/*
			 * search for 'term' in builtin_termcaps[]
			 */
			termp = find_builtin_term(term);
			if (termp->bt_string == NULL)		/* did not find it */
			{
#ifdef HAVE_TGETENT
				/*
				 * If try == 0, first try the external termcap. If that is not
				 * found we'll get back here with try == 2.
				 * If termcap_cleared is set we used the external termcap,
				 * don't complain about not finding the term in the builtin
				 * termcap.
				 */
				if (try == 0)					/* try external one */
					continue;
				if (termcap_cleared)			/* found in external termcap */
					break;
#endif

				fprintf(stderr, "\r\n");
				if (error_msg != NULL)
				{
					fprintf(stderr, (char *)error_msg);
					fprintf(stderr, "\r\n");
				}
				fprintf(stderr, "'%s' not known. Available builtin terminals are:\r\n", term);
				for (termp = &(builtin_termcaps[0]); termp->bt_string != NULL;
																	  ++termp)
				{
					if (termp->bt_entry == KS_NAME)
#ifdef HAVE_TGETENT
						fprintf(stderr, "    builtin_%s\r\n", termp->bt_string);
#else
						fprintf(stderr, "    %s\r\n", termp->bt_string);
#endif
				}
				if (!starting)	/* when user typed :set term=xxx, quit here */
				{
					screen_start();		/* don't know where cursor is now */
					wait_return(TRUE);
					return FAIL;
				}
				term = DEFAULT_TERM;
				fprintf(stderr, "defaulting to '%s'\r\n", term);
				screen_start();			/* don't know where cursor is now */
				mch_delay(2000L, TRUE);
				set_string_option((char_u *)"term", -1, term, TRUE);
			}
			flushbuf();
#ifdef HAVE_TGETENT
			if (!termcap_cleared)
			{
#endif
				clear_termoptions();		/* clear old options */
#ifdef HAVE_TGETENT
				termcap_cleared = TRUE;
			}
#endif
			parse_builtin_tcap(term);
#ifdef USE_GUI
			if (STRCMP(term, "gui") == 0)
			{
				flushbuf();
				settmode(0);
				gui_init();
				if (!gui.in_use)		/* failed to start GUI */
					settmode(1);
			}
#endif /* USE_GUI */
		}
#ifdef HAVE_TGETENT
	}
#endif

/*
 * special: There is no info in the termcap about whether the cursor
 * positioning is relative to the start of the screen or to the start of the
 * scrolling region.  We just guess here. Only msdos pcterm is known to do it
 * relative.
 */
	if (STRCMP(term, "pcterm") == 0)
		T_CSC = (char_u *)"yes";
	else
		T_CSC = empty_option;

#ifdef UNIX
/*
 * Any "stty" settings override the default for t_kb from the termcap.
 * This is in unix.c, because it depends a lot on the version of unix that is
 * being used.
 * Don't do this when the GUI is active, it uses "t_kb" and "t_kD" directly.
 */
#ifdef USE_GUI
	if (!gui.in_use)
#endif
		get_stty();
#endif

/*
 * If the termcap has no entry for 'bs' and/or 'del' and the ioctl() also
 * didn't work, use the default CTRL-H
 * The default for t_kD is DEL, unless t_kb is DEL.
 * The strsave'd strings are probably lost forever, well it's only two bytes.
 * Don't do this when the GUI is active, it uses "t_kb" and "t_kD" directly.
 */
#ifdef USE_GUI
	if (!gui.in_use)
#endif
	{
		bs_p = find_termcode((char_u *)"kb");
		del_p = find_termcode((char_u *)"kD");
		if (bs_p == NULL || *bs_p == NUL)
			add_termcode((char_u *)"kb", (bs_p = (char_u *)"\010"));
		if ((del_p == NULL || *del_p == NUL) &&
											(bs_p == NULL || *bs_p != '\177'))
			add_termcode((char_u *)"kD", (char_u *)"\177");
	}

#ifdef USE_MOUSE
	/*
	 * recognize mouse events in the input stream for xterm, msdos and win32
	 */
	{	
		char_u	name[2];

		name[0] = KS_MOUSE;
		name[1] = K_FILLER;
# ifdef UNIX
		if (is_xterm(term))
			add_termcode(name, (char_u *)"\033[M");
# else
		add_termcode(name, (char_u *)"\233M");
# endif
	}
#endif

#if defined(AMIGA) || defined(MSDOS) || defined(WIN32) || defined(OS2)
		/* DEFAULT_TERM indicates that it is the machine console. */
	if (STRCMP(term, DEFAULT_TERM))
		term_console = FALSE;
	else
	{
		term_console = TRUE;
# ifdef AMIGA
		win_resize_on();		/* enable window resizing reports */
# endif
	}
#endif

#ifdef UNIX
/*
 * 'ttyfast' is default on for xterm, iris-ansi and a few others.
 */
	if (is_fastterm(term))
		p_tf = TRUE;
#endif
#if defined(AMIGA) || defined(MSDOS) || defined(WIN32) || defined(OS2)
/*
 * 'ttyfast' is default on Amiga, MSDOS, Win32, and OS/2 consoles
 */
	if (term_console)
		p_tf = TRUE;
#endif

	ttest(TRUE);		/* make sure we have a valid set of terminal codes */
	set_term_defaults();	/* use current values as defaults */

	/*
	 * Initialize the terminal with the appropriate termcap codes.
	 * Set the mouse and window title if possible.
	 * Don't do this when starting, need to parse the .vimrc first, because it
	 * may redefine t_TI etc.
	 */
	if (!starting)
	{
		starttermcap();			/* may change terminal mode */
#ifdef USE_MOUSE
		setmouse();				/* may start using the mouse */
#endif
		maketitle();			/* may display window title */
	}

		/* display initial screen after ttest() checking. jw. */
	if (width <= 0 || height <= 0)
	{
		/* termcap failed to report size */
		/* set defaults, in case mch_get_winsize also fails */
		width = 80;
#if defined MSDOS  ||  defined WIN32
		height = 25;		/* console is often 25 lines */
#else
		height = 24;		/* most terminals are 24 lines */
#endif
	}
	set_winsize(width, height, FALSE);	/* may change Rows */
	if (!starting)
	{
		if (scroll_region)
			scroll_region_reset();			/* In case Rows changed */
		check_map_keycodes();	/* check mappings for terminal codes used */
	}

	return OK;
}

#ifdef HAVE_TGETENT
/*
 * Call tgetent()
 * Return error message if it fails, NULL if it's OK.
 */
	static char_u *
tgetent_error(tbuf, term)
	char_u	*tbuf;
	char_u	*term;
{
	int		i;

	i = TGETENT(tbuf, term);
	if (i == -1)
		return (char_u *)"Cannot open termcap file";
	if (i == 0)
#ifdef TERMINFO
		return (char_u *)"Terminal entry not found in terminfo";
#else
		return (char_u *)"Terminal entry not found in termcap";
#endif
	return NULL;
}
#endif /* HAVE_TGETENT */

#if defined(HAVE_TGETENT) && (defined(UNIX) || defined(__EMX__))
/*
 * Get Columns and Rows from the termcap. Used after a window signal if the
 * ioctl() fails. It doesn't make sense to call tgetent each time if the "co"
 * and "li" entries never change. But on some systems this works.
 * Errors while getting the entries are ignored.
 */
	void
getlinecol()
{
	char_u			tbuf[TBUFSZ];

	if (term_strings[KS_NAME] != NULL && TGETENT(tbuf, term_strings[KS_NAME]) > 0)
	{
		if (Columns == 0)
			Columns = tgetnum("co");
		if (Rows == 0)
			Rows = tgetnum("li");
	}
}
#endif /* defined(HAVE_TGETENT) && defined(UNIX) */

/*
 * Get a string entry from the termcap and add it to the list of termcodes.
 * Used for <t_xx> special keys.
 * Give an error message for failure when not sourcing.
 * If force given, replace an existing entry.
 * Return FAIL if the entry was not found, OK if the entry was added.
 */
	int
add_termcap_entry(name, force)
	char_u	*name;
	int		force;
{
	char_u	*term;
	int		key;
	struct builtin_term *termp;
#ifdef HAVE_TGETENT
	char_u	*string;
	int		i;
	int		builtin_first;
	char_u	tbuf[TBUFSZ];
	char_u	tstrbuf[TBUFSZ];
	char_u	*tp = tstrbuf;
	char_u	*error_msg = NULL;
#endif

/*
 * If the GUI is running or will start in a moment, we only support the keys
 * that the GUI can produce.
 */
#ifdef USE_GUI
	if (gui.in_use || gui.starting)
		return gui_mch_haskey(name);
#endif

	if (!force && find_termcode(name) != NULL)		/* it's already there */
		return OK;

	term = term_strings[KS_NAME];
	if (term == NULL)						/* just in case */
		return FAIL;

	if (is_builtin_term(term))				/* name starts with "builtin_" */
	{
		term += 8;
#ifdef HAVE_TGETENT
		builtin_first = TRUE;
#endif
	}
#ifdef HAVE_TGETENT
	else
		builtin_first = p_tbi;
#endif

#ifdef HAVE_TGETENT
/*
 * We can get the entry from the builtin termcap and from the external one.
 * If 'ttybuiltin' is on or the terminal name starts with "builtin_", try
 * builtin termcap first.
 * If 'ttybuiltin' is off, try external termcap first.
 */
	for (i = 0; i < 2; ++i)
	{
		if (!builtin_first == i)
#endif
		/*
		 * Search in builtin termcap
		 */
		{
			termp = find_builtin_term(term);
			if (termp->bt_string != NULL)		/* found it */
			{
				key = TERMCAP2KEY(name[0], name[1]);
				while (termp->bt_entry != KS_NAME)
				{
					if (termp->bt_entry == key)
					{
						add_termcode(name, (char_u *)termp->bt_string);
						return OK;
					}
					++termp;
				}
			}
		}
#ifdef HAVE_TGETENT
		else
		/*
		 * Search in external termcap
		 */
		{
			error_msg = tgetent_error(tbuf, term);
			if (error_msg == NULL)
			{
				string = TGETSTR((char *)name, &tp);
				if (string != NULL && *string != NUL)
				{
					add_termcode(name, string);
					return OK;
				}
			}
		}
	}
#endif

	if (sourcing_name == NULL)
	{
#ifdef HAVE_TGETENT
		if (error_msg != NULL)
			EMSG(error_msg);
		else
#endif
			EMSG2("No \"%s\" entry in termcap", name);
	}
	return FAIL;
}

	static int
is_builtin_term(name)
	char_u	*name;
{
	return (STRNCMP(name, "builtin_", (size_t)8) == 0);
}

static char_u *tltoa __PARMS((unsigned long));

	static char_u *
tltoa(i)
	unsigned long i;
{
	static char_u buf[16];
	char_u		*p;

	p = buf + 15;
	*p = '\0';
	do
	{
		--p;
		*p = (char_u) (i % 10 + '0');
		i /= 10;
	}
	while (i > 0 && p > buf);
	return p;
}

#ifndef HAVE_TGETENT

/*
 * minimal tgoto() implementation.
 * no padding and we only parse for %i %d and %+char
 */
char *tgoto __ARGS((char *, int, int));

	char *
tgoto(cm, x, y)
	char *cm;
	int x, y;
{
	static char buf[30];
	char *p, *s, *e;

	if (!cm)
		return "OOPS";
	e = buf + 29;
	for (s = buf; s < e && *cm; cm++)
	{
		if (*cm != '%')
		{
			*s++ = *cm;
			continue;
		}
		switch (*++cm)
		{
		case 'd':
			p = (char *)tltoa((unsigned long)y);
			y = x;
			while (*p)
				*s++ = *p++;
			break;
		case 'i':
			x++;
			y++;
			break;
		case '+':
			*s++ = (char)(*++cm + y);
			y = x;
			break;
		case '%':
			*s++ = *cm;
			break;
		default:
			return "OOPS";
		}
	}
	*s = '\0';
	return buf;
}

#endif /* HAVE_TGETENT */

/*
 * Set the terminal name to "term" and initialize the terminal options.
 * If "term" is NULL or empty, get the terminal name from the environment.
 * If that fails, use the default terminal name.
 */
	void
termcapinit(term)
	char_u *term;
{
#ifndef WIN32
	if (!term || !*term)
		term = vim_getenv((char_u *)"TERM");
#endif
	if (!term || !*term)
		term = DEFAULT_TERM;
	set_string_option((char_u *)"term", -1, term, TRUE);
	/*
	 * Avoid using "term" here, because the next vim_getenv() may overwrite it.
	 */
	set_termname(term_strings[KS_NAME] != NULL ? term_strings[KS_NAME] : term);
}

/*
 * the number of calls to mch_write is reduced by using the buffer "outbuf"
 */
#undef BSIZE			/* hpux has BSIZE in sys/option.h */
#define BSIZE	2048
static char_u			outbuf[BSIZE];
static int				bpos = 0;		/* number of chars in outbuf */

/*
 * flushbuf(): flush the output buffer
 */
	void
flushbuf()
{
	if (bpos != 0)
	{
		mch_write(outbuf, bpos);
		bpos = 0;
	}
}

#ifdef USE_GUI
/*
 * trash_output_buf(): Throw away the contents of the output buffer
 */
	void
trash_output_buf()
{
	bpos = 0;
}
#endif

/*
 * outchar(c): put a character into the output buffer.
 *			   Flush it if it becomes full.
 * This should not be used for outputting text on the screen (use functions like
 * msg_outstr() and screen_outchar() for that).
 */
	void
outchar(c)
	unsigned	c;
{
#if defined(UNIX) || defined(VMS) || defined(AMIGA)
	if (c == '\n')		/* turn LF into CR-LF (CRMOD doesn't seem to do this) */
		outchar('\r');
#endif

	outbuf[bpos++] = c;

	/* For testing we flush each time. */
	if (bpos >= BSIZE || p_wd)
		flushbuf();
}

static void outchar_nf __ARGS((unsigned));

/*
 * outchar_nf(c): like outchar(), but don't flush when p_wd is set
 */
	static void
outchar_nf(c)
	unsigned	c;
{
#if defined(UNIX) || defined(VMS) || defined(AMIGA)
	if (c == '\n')		/* turn LF into CR-LF (CRMOD doesn't seem to do this) */
		outchar_nf('\r');
#endif

	outbuf[bpos++] = c;

	/* For testing we flush each time. */
	if (bpos >= BSIZE)
		flushbuf();
}

/*
 * a never-padding outstr.
 * use this whenever you don't want to run the string through tputs.
 * tputs above is harmless, but tputs from the termcap library
 * is likely to strip off leading digits, that it mistakes for padding
 * information. (jw)
 * This should only be used for writing terminal codes, not for outputting
 * normal text (use functions like msg_outstr() and screen_outchar() for that).
 */
	void
outstrn(s)
	char_u *s;
{
	if (bpos > BSIZE - 20)		/* avoid terminal strings being split up */
		flushbuf();
	while (*s)
		outchar_nf(*s++);

	/* For testing we write one string at a time. */
	if (p_wd)
		flushbuf();
}

/*
 * outstr(s): put a string character at a time into the output buffer.
 * If HAVE_TGETENT is defined use the termcap parser. (jw)
 * This should only be used for writing terminal codes, not for outputting
 * normal text (use functions like msg_outstr() and screen_outchar() for that).
 */
	void
outstr(s)
	register char_u			 *s;
{
	if (bpos > BSIZE - 20)		/* avoid terminal strings being split up */
		flushbuf();
	if (s)
#ifdef HAVE_TGETENT
		tputs((char *)s, 1, TPUTSFUNCAST outchar_nf);
#else
		while (*s)
			outchar_nf(*s++);
#endif

	/* For testing we write one string at a time. */
	if (p_wd)
		flushbuf();
}

/*
 * cursor positioning using termcap parser. (jw)
 */
	void
windgoto(row, col)
	int		row;
	int		col;
{
	if (col != screen_cur_col || row != screen_cur_row)
	{
		/*
		 * When positioning on the same row, column 0, use CR, it's just one
		 * character.  Don't do this when the cursor has moved past the end of
		 * the line, the cursor position is undefined then.
		 */
		if (row == screen_cur_row && col == 0 && screen_cur_col < Columns)
		{
			outchar('\r');
			screen_cur_col = 0;
		}
		else
		{
			OUTSTR(tgoto((char *)T_CM, col, row));
			screen_cur_col = col;
			screen_cur_row = row;
		}
	}
}

/*
 * Set cursor to current position.
 */

	void
setcursor()
{
	if (!RedrawingDisabled)
		windgoto(curwin->w_winpos + curwin->w_row,
#ifdef RIGHTLEFT
				curwin->w_p_rl ? (int)Columns - 1 - curwin->w_col :
#endif
															curwin->w_col);
}

/*
 * Make sure we have a valid set or terminal options.
 * Replace all entries that are NULL by empty_option
 */
	void
ttest(pairs)
	int	pairs;
{
	char	*t = NULL;

	check_options();				/* make sure no options are NULL */

  /* hard requirements */
	if (*T_CL == NUL)				/* erase display */
		t = "cl";
	if (*T_CM == NUL)				/* cursor motion */
		t = "cm";

	if (t)
		EMSG2("terminal capability %s required", t);

/*
 * if "cs" defined, use a scroll region, it's faster.
 */
	if (*T_CS != NUL)
		scroll_region = TRUE;
	else
		scroll_region = FALSE;

	if (pairs)
	{
	  /* optional pairs */
			/* TP goes to normal mode for TI (invert) and TB (bold) */
		if (*T_ME == NUL)
			T_ME = T_MR = T_MD = empty_option;
		if (*T_SO == NUL || *T_SE == NUL)
			T_SO = T_SE = empty_option;
		if (*T_US == NUL || *T_UE == NUL)
			T_US = T_UE = empty_option;
		if (*T_CZH == NUL || *T_CZR == NUL)
			T_CZH = T_CZR = empty_option;
			/* T_VE is needed even though T_VI is not defined */
		if (*T_VE == NUL)
			T_VI = empty_option;
			/* if 'mr' or 'me' is not defined use 'so' and 'se' */
		if (*T_ME == NUL)
		{
			T_ME = T_SE;
			T_MR = T_SO;
			T_MD = T_SO;
		}
			/* if 'so' or 'se' is not defined use 'mr' and 'me' */
		if (*T_SO == NUL)
		{
			T_SE = T_ME;
			if (*T_MR == NUL)
				T_SO = T_MD;
			else
				T_SO = T_MR;
		}
			/* if 'ZH' or 'ZR' is not defined use 'mr' and 'me' */
		if (*T_CZH == NUL)
		{
			T_CZR = T_ME;
			if (*T_MR == NUL)
				T_CZH = T_MD;
			else
				T_CZH = T_MR;
		}
	}
	need_gather = TRUE;
}

/*
 * Represent the given long_u as individual bytes, with the most significant
 * byte first, and store them in dst.
 */
	void
add_long_to_buf(val, dst)
	long_u	val;
	char_u	*dst;
{
	int		i;
	int		shift;

	for (i = 1; i <= sizeof(long_u); i++)
	{
		shift = 8 * (sizeof(long_u) - i);
		dst[i - 1] = (char_u) ((val >> shift) & 0xff);
	}
}

/*
 * Interpret the next string of bytes in buf as a long integer, with the most
 * significant byte first.	Note that it is assumed that buf has been through
 * inchar(), so that NUL and K_SPECIAL will be represented as three bytes each.
 * Puts result in val, and returns the number of bytes read from buf
 * (between sizeof(long_u) and 2 * sizeof(long_u)), or -1 if not enough bytes
 * were present.
 */
	int
get_long_from_buf(buf, val)
	char_u	*buf;
	long_u	*val;
{
	int		len;
	char_u	bytes[sizeof(long_u)];
	int		i;
	int		shift;

	*val = 0;
	len = get_bytes_from_buf(buf, bytes, (int)sizeof(long_u));
	if (len != -1)
	{
		for (i = 0; i < sizeof(long_u); i++)
		{
			shift = 8 * (sizeof(long_u) - 1 - i);
			*val += (long_u)bytes[i] << shift;
		}
	}
	return len;
}

/*
 * Read the next num_bytes bytes from buf, and store them in bytes.  Assume
 * that buf has been through inchar().	Returns the actual number of bytes used
 * from buf (between num_bytes and num_bytes*2), or -1 if not enough bytes were
 * available.
 */
	static int
get_bytes_from_buf(buf, bytes, num_bytes)
	char_u	*buf;
	char_u	*bytes;
	int		num_bytes;
{
	int		len = 0;
	int		i;
	char_u	c;

	for (i = 0; i < num_bytes; i++)
	{
		if ((c = buf[len++]) == NUL)
			return -1;
		if (c == K_SPECIAL)
		{
			if (buf[len] == NUL || buf[len + 1] == NUL)		/* cannot happen? */
				return -1;
			if (buf[len++] == KS_ZERO)
				c = NUL;
			++len;		/* skip K_FILLER */
			/* else it should be KS_SPECIAL, and c already equals K_SPECIAL */
		}
		bytes[i] = c;
	}
	return len;
}

/*
 * outnum - output a (big) number fast
 */
	void
outnum(n)
	register long n;
{
	OUTSTRN(tltoa((unsigned long)n));
}

	void
check_winsize()
{
	static int	old_Rows = 0;

	if (Columns < MIN_COLUMNS)
		Columns = MIN_COLUMNS;
	if (Rows < min_rows())		/* need room for one window and command line */
		Rows = min_rows();

	if (old_Rows != Rows)
	{
		old_Rows = Rows;
		screen_new_rows();			/* may need to update window sizes */
	}
}

/*
 * set window size
 * If 'mustset' is TRUE, we must set Rows and Columns, do not get real
 * window size (this is used for the :win command).
 * If 'mustset' is FALSE, we may try to get the real window size and if
 * it fails use 'width' and 'height'.
 */
	void
set_winsize(width, height, mustset)
	int		width, height;
	int		mustset;
{
	register int		tmp;

	if (width < 0 || height < 0)	/* just checking... */
		return;

									/* postpone the resizing */
	if (State == HITRETURN || State == SETWSIZE)
	{
		State = SETWSIZE;
		return;
	}
	if (State != ASKMORE && State != EXTERNCMD)
		screenclear();
	else
		screen_start();					/* don't know where cursor is now */
#ifdef AMIGA
	flushbuf();			/* must do this before mch_get_winsize for some
							obscure reason */
#endif /* AMIGA */
	if (mustset || (mch_get_winsize() == FAIL && height != 0))
	{
		Rows = height;
		Columns = width;
		check_winsize();		/* always check, to get p_scroll right */
		mch_set_winsize();
	}
	else
		check_winsize();		/* always check, to get p_scroll right */
	if (!starting)
	{
		comp_Botline_all();
		maketitle();
		if (State == ASKMORE || State == EXTERNCMD)
		{
			screenalloc(FALSE);	/* don't redraw, just adjust screen size */
			if (State == ASKMORE)
			{
				msg_moremsg(FALSE);	/* display --more-- message again */
				msg_row = Rows - 1;
			}
			else
				windgoto(msg_row, msg_col);	/* put cursor back */
		}
		else
		{
			tmp = RedrawingDisabled;
			RedrawingDisabled = FALSE;
			updateScreen(CURSUPD);
			RedrawingDisabled = tmp;
			if (State == CMDLINE)
				redrawcmdline();
			else
				setcursor();
		}
	}
	flushbuf();
}

	void
settmode(raw)
	int	 raw;
{
	static int	oldraw = FALSE;

#ifdef USE_GUI
	/* don't set the term where gvim was started in raw mode */
	if (gui.in_use)
		return;
#endif
	if (full_screen)
	{
		/*
		 * When returning after calling a shell we want to really set the
		 * terminal to raw mode, even though we think it already is, because
		 * the shell program may have reset the terminal mode.
		 * When we think the terminal is not-raw, don't try to set it to
		 * not-raw again, because that causes problems (logout!) on some
		 * machines.
		 */
		if (raw || oldraw)
		{
			flushbuf();
			mch_settmode(raw);	/* machine specific function */
#ifdef USE_MOUSE
			if (!raw)
				mch_setmouse(FALSE);			/* switch mouse off */
			else
				setmouse();						/* may switch mouse on */
#endif
			flushbuf();
			oldraw = raw;
		}
	}
}

	void
starttermcap()
{
	if (full_screen && !termcap_active)
	{
		outstr(T_TI);					/* start termcap mode */
		outstr(T_KS);					/* start "keypad transmit" mode */
		flushbuf();
		termcap_active = TRUE;
		screen_start();					/* don't know where cursor is now */
	}
}

	void
stoptermcap()
{
	if (full_screen && termcap_active)
	{
		outstr(T_KE);					/* stop "keypad transmit" mode */
		flushbuf();
		termcap_active = FALSE;
		cursor_on();					/* just in case it is still off */
		outstr(T_TE);					/* stop termcap mode */
		screen_start();					/* don't know where cursor is now */
	}
}

#ifdef USE_MOUSE
/*
 * setmouse() - switch mouse on/off depending on current mode and 'mouse'
 */
	void
setmouse()
{
	int		checkfor;

# ifdef USE_GUI
	if (gui.in_use)
		return;
# endif
	if (*p_mouse == NUL)			/* be quick when mouse is off */
		return;

	if (VIsual_active)
		checkfor = MOUSE_VISUAL;
	else if (State == HITRETURN)
		checkfor = MOUSE_RETURN;
	else if (State & INSERT)
		checkfor = MOUSE_INSERT;
	else if (State & CMDLINE)
		checkfor = MOUSE_COMMAND;
	else
		checkfor = MOUSE_NORMAL;				/* assume normal mode */
	
	if (mouse_has(checkfor))
		mch_setmouse(TRUE);
	else
		mch_setmouse(FALSE);
}

/*
 * Return TRUE if
 * - "c" is in 'mouse', or
 * - 'a' is in 'mouse' and "c" is in MOUSE_A, or
 * - the current buffer is a help file and 'h' is in 'mouse' and we are in a
 *   normal editing mode (not at hit-return message).
 */
	int
mouse_has(c)
	int		c;
{
	return (vim_strchr(p_mouse, c) != NULL ||
					(vim_strchr(p_mouse, 'a') != NULL &&
								  vim_strchr((char_u *)MOUSE_A, c) != NULL) ||
					(c != MOUSE_RETURN && curbuf->b_help &&
											vim_strchr(p_mouse, MOUSE_HELP)));
}
#endif

/*
 * By outputting the 'cursor very visible' termcap code, for some windowed
 * terminals this makes the screen scrolled to the correct position.
 * Used when starting Vim or returning from a shell.
 */
	void
scroll_start()
{
	if (*T_VS != NUL)
	{
		outstr(T_VS);
		outstr(T_VE);
		screen_start();					/* don't know where cursor is now */
	}
}

/*
 * enable cursor, unless in Visual mode or no inversion possible
 */
static int cursor_is_off = FALSE;

	void
cursor_on()
{
	if (full_screen)
	{
		if (cursor_is_off && (!VIsual_active || highlight == NULL))
		{
			outstr(T_VE);
			cursor_is_off = FALSE;
		}
	}
}

	void
cursor_off()
{
	if (full_screen)
	{
		if (!cursor_is_off)
			outstr(T_VI);			/* disable cursor */
		cursor_is_off = TRUE;
	}
}

/*
 * Set scrolling region for window 'wp'.
 * The region starts 'off' lines from the start of the window.
 */
	void
scroll_region_set(wp, off)
	WIN		*wp;
	int		off;
{
	OUTSTR(tgoto((char *)T_CS, wp->w_winpos + wp->w_height - 1,
														 wp->w_winpos + off));
	screen_start();					/* don't know where cursor is now */
}

/*
 * Reset scrolling region to the whole screen.
 */
	void
scroll_region_reset()
{
	OUTSTR(tgoto((char *)T_CS, (int)Rows - 1, 0));
	screen_start();					/* don't know where cursor is now */
}


/*
 * List of terminal codes that are currently recognized.
 */

struct termcode
{
	char_u	name[2];		/* termcap name of entry */
	char_u	*code;			/* terminal code (in allocated memory) */
	int		len;			/* STRLEN(code) */
} *termcodes = NULL;

static int	tc_max_len = 0;	/* number of entries that termcodes[] can hold */
static int	tc_len = 0;		/* current number of entries in termcodes[] */

	void
clear_termcodes()
{
	while (tc_len > 0)
		vim_free(termcodes[--tc_len].code);
	vim_free(termcodes);
	termcodes = NULL;
	tc_max_len = 0;

#ifdef HAVE_TGETENT
	BC = (char *)empty_option;
	UP = (char *)empty_option;
	PC = NUL;					/* set pad character to NUL */
	ospeed = 0;
#endif

	need_gather = TRUE;			/* need to fill termleader[] */
}

/*
 * Add a new entry to the list of terminal codes.
 * The list is kept alphabetical for ":set termcap"
 */
	void
add_termcode(name, string)
	char_u	*name;
	char_u	*string;
{
	struct termcode *new_tc;
	int				i, j;
	char_u			*s;

	if (string == NULL || *string == NUL)
	{
		del_termcode(name);
		return;
	}

	s = strsave(string);
	if (s == NULL)
		return;

	need_gather = TRUE;			/* need to fill termleader[] */

	/*
	 * need to make space for more entries
	 */
	if (tc_len == tc_max_len)
	{
		tc_max_len += 20;
		new_tc = (struct termcode *)alloc(
							(unsigned)(tc_max_len * sizeof(struct termcode)));
		if (new_tc == NULL)
		{
			tc_max_len -= 20;
			return;
		}
		for (i = 0; i < tc_len; ++i)
			new_tc[i] = termcodes[i];
		vim_free(termcodes);
		termcodes = new_tc;
	}

	/*
	 * Look for existing entry with the same name, it is replaced.
	 * Look for an existing entry that is alphabetical higher, the new entry
	 * is inserted in front of it.
	 */
	for (i = 0; i < tc_len; ++i)
	{
		if (termcodes[i].name[0] < name[0])
			continue;
		if (termcodes[i].name[0] == name[0])
		{
			if (termcodes[i].name[1] < name[1])
				continue;
			/*
			 * Exact match: Replace old code.
			 */
			if (termcodes[i].name[1] == name[1])
			{
				vim_free(termcodes[i].code);
				--tc_len;
				break;
			}
		}
		/*
		 * Found alphabetical larger entry, move rest to insert new entry
		 */
		for (j = tc_len; j > i; --j)
			termcodes[j] = termcodes[j - 1];
		break;
	}

	termcodes[i].name[0] = name[0];
	termcodes[i].name[1] = name[1];
	termcodes[i].code = s;
	termcodes[i].len = STRLEN(s);
	++tc_len;
}

	char_u	*
find_termcode(name)
	char_u	*name;
{
	int		i;

	for (i = 0; i < tc_len; ++i)
		if (termcodes[i].name[0] == name[0] && termcodes[i].name[1] == name[1])
			return termcodes[i].code;
	return NULL;
}

	char_u *
get_termcode(i)
	int		i;
{
	if (i >= tc_len)
		return NULL;
	return &termcodes[i].name[0];
}

	void
del_termcode(name)
	char_u	*name;
{
	int		i;

	if (termcodes == NULL)		/* nothing there yet */
		return;

	need_gather = TRUE;			/* need to fill termleader[] */

	for (i = 0; i < tc_len; ++i)
		if (termcodes[i].name[0] == name[0] && termcodes[i].name[1] == name[1])
		{
			vim_free(termcodes[i].code);
			--tc_len;
			while (i < tc_len)
			{
				termcodes[i] = termcodes[i + 1];
				++i;
			}
			return;
		}
	/* not found. Give error message? */
}

/*
 * Check if typebuf[] contains a terminal key code.
 * Check from typebuf[typeoff] to typebuf[typeoff + max_offset].
 * Return 0 for no match, -1 for partial match, > 0 for full match.
 * With a match, the match is removed, the replacement code is inserted in
 * typebuf[] and the number of characters in typebuf[] is returned.
 */
	int
check_termcode(max_offset)
	int		max_offset;
{
	register char_u		*tp;
	register char_u		*p;
	int			slen = 0;		/* init for GCC */
	int			len;
	int			offset;
	char_u		key_name[2];
	int			new_slen;
	int			extra;
	char_u		string[MAX_KEY_CODE_LEN + 1];
	int			i, j;
#ifdef USE_GUI
	long_u		val;
#endif
#ifdef USE_MOUSE
	char_u		bytes[3];
	int			num_bytes;
	int			mouse_code;
	int			modifiers;
	int			is_click, is_drag;
	int			current_button;
	static int	held_button = MOUSE_RELEASE;
	static int	orig_num_clicks = 1;
	static int	orig_mouse_code = 0x0;
# if defined(UNIX) && defined(HAVE_GETTIMEOFDAY) && defined(HAVE_SYS_TIME_H)
	static int	orig_mouse_col = 0;
	static int	orig_mouse_row = 0;
	static linenr_t	orig_topline = 0;
	static struct timeval  orig_mouse_time = {0, 0};
										/* time of previous mouse click */
	struct timeval  mouse_time;			/* time of current mouse click */
	long		timediff;				/* elapsed time in msec */
# endif
#endif

	/*
	 * Speed up the checks for terminal codes by gathering all first bytes
	 * used in termleader[].  Often this is just a single <Esc>.
	 */
	if (need_gather)
		gather_termleader();

	/*
	 * Check at several positions in typebuf[], to catch something like
	 * "x<Up>" that can be mapped. Stop at max_offset, because characters
	 * after that cannot be used for mapping, and with @r commands typebuf[]
	 * can become very long.
	 * This is used often, KEEP IT FAST!
	 */
	for (offset = 0; offset < typelen && offset < max_offset; ++offset)
	{
		tp = typebuf + typeoff + offset;

		/*
		 * Don't check characters after K_SPECIAL, those are already
		 * translated terminal chars (avoid translating ~@^Hx).
		 */
		if (*tp == K_SPECIAL)
		{
			offset += 2;		/* there are always 2 extra characters */
			continue;
		}

		/*
		 * Skip this position if the character does not appear as the first
		 * character in term_strings. This speeds up a lot, since most
		 * termcodes start with the same character (ESC or CSI).
		 */
		i = *tp;
		for (p = termleader; *p && *p != i; ++p)
			;
		if (*p == NUL)
			continue;

		/*
		 * Skip this position if p_ek is not set and
		 * typebuf[typeoff + offset] is an ESC and we are in insert mode
		 */
		if (*tp == ESC && !p_ek && (State & INSERT))
			continue;

		len = typelen - offset;	/* length of the input */
		new_slen = 0;			/* Length of what will replace the termcode */
		key_name[0] = NUL;		/* no key name found yet */

#ifdef USE_GUI
		if (gui.in_use)
		{
			/*
			 * GUI special key codes are all of the form [CSI xx].
			 */
			if (*tp == CSI)			/* Special key from GUI */
			{
				if (len < 3)
					return -1;		/* Shouldn't happen */
				slen = 3;
				key_name[0] = tp[1];
				key_name[1] = tp[2];
			}
		}
		else
#endif /* USE_GUI */
		{
			for (i = 0; i < tc_len; ++i)
			{
				/*
				 * Ignore the entry if we are not at the start of typebuf[]
				 * and there are not enough characters to make a match.
				 */
				slen = termcodes[i].len;
				if (offset && len < slen)
					continue;
				if (STRNCMP(termcodes[i].code, tp,
									 (size_t)(slen > len ? len : slen)) == 0)
				{
					if (len < slen)				/* got a partial sequence */
						return -1;				/* need to get more chars */

					/*
					 * When found a keypad key, check if there is another key
					 * that matches and use that one.  This makes <Home> to be
					 * found instead of <kHome> when they produce the same
					 * key code.
					 */
					if (termcodes[i].name[0] == 'K' &&
												isdigit(termcodes[i].name[1]))
					{
						for (j = i + 1; j < tc_len; ++j)
							if (termcodes[j].len == slen &&
									STRNCMP(termcodes[i].code,
											termcodes[j].code, slen) == 0)
							{
								i = j;
								break;
							}
					}

					key_name[0] = termcodes[i].name[0];
					key_name[1] = termcodes[i].name[1];

					/*
					 * If it's a shifted special key, then include the SHIFT
					 * modifier
					 */
					if (unshift_special_key(&key_name[0]))
					{
						string[new_slen++] = K_SPECIAL;
						string[new_slen++] = KS_MODIFIER;
						string[new_slen++] = MOD_MASK_SHIFT;
					}
					break;
				}
			}
		}

		if (key_name[0] == NUL)
			continue;		/* No match at this position, try next one */

		/* We only get here when we have a complete termcode match */

#ifdef USE_MOUSE
		/*
		 * If it is a mouse click, get the coordinates.
		 * we get "<t_mouse>scr", where
		 *	s == encoded mouse button state (0x20 = left, 0x22 = right, etc)
		 *	c == column + ' ' + 1 == column + 33
		 *	r == row + ' ' + 1 == row + 33
		 *
		 * The coordinates are passed on through global variables. Ugly,
		 * but this avoids trouble with mouse clicks at an unexpected
		 * moment and allows for mapping them.
		 */
		if (key_name[0] == KS_MOUSE)
		{
			num_bytes = get_bytes_from_buf(tp + slen, bytes, 3);
			if (num_bytes == -1)	/* not enough coordinates */
				return -1;
			mouse_code = bytes[0];
			mouse_col = bytes[1] - ' ' - 1;
			mouse_row = bytes[2] - ' ' - 1;
			slen += num_bytes;

			/* Interpret the mouse code */
			is_click = is_drag = FALSE;
			current_button = (mouse_code & MOUSE_CLICK_MASK);
			if (current_button == MOUSE_RELEASE)
			{
				/*
				 * If we get a mouse drag or release event when
				 * there is no mouse button held down (held_button ==
				 * MOUSE_RELEASE), produce a K_IGNORE below.
				 * (can happen when you hold down two buttons
				 * and then let them go, or click in the menu bar, but not
				 * on a menu, and drag into the text).
				 */
				if ((mouse_code & MOUSE_DRAG) == MOUSE_DRAG)
					is_drag = TRUE;
				current_button = held_button;
			}
			else
			{
#if defined(UNIX) && defined(HAVE_GETTIMEOFDAY) && defined(HAVE_SYS_TIME_H)
# ifdef USE_GUI
				/*
				 * Only for Unix, when GUI is not active, we handle
				 * multi-clicks here.
				 */
				if (!gui.in_use)
# endif
				{
					/*
					 * Compute the time elapsed since the previous mouse click.
					 */
					gettimeofday(&mouse_time, NULL);
					timediff = (mouse_time.tv_usec -
											  orig_mouse_time.tv_usec) / 1000;
					if (timediff < 0)
						--orig_mouse_time.tv_sec;
					timediff += (mouse_time.tv_sec -
											   orig_mouse_time.tv_sec) * 1000;
					orig_mouse_time = mouse_time;
					if (mouse_code == orig_mouse_code &&
							timediff < p_mouset &&
							orig_num_clicks != 4 &&
							orig_mouse_col == mouse_col &&
							orig_mouse_row == mouse_row &&
							orig_topline == curwin->w_topline)
						++orig_num_clicks;
					else
						orig_num_clicks = 1;
					orig_mouse_col = mouse_col;
					orig_mouse_row = mouse_row;
					orig_topline = curwin->w_topline;
				}
# ifdef USE_GUI
				else
					orig_num_clicks = NUM_MOUSE_CLICKS(mouse_code);
# endif
#else
				orig_num_clicks = NUM_MOUSE_CLICKS(mouse_code);
#endif
				is_click = TRUE;
				orig_mouse_code = mouse_code;
			}
			if (!is_drag)
				held_button = mouse_code & MOUSE_CLICK_MASK;

			/*
			 * Translate the actual mouse event into a pseudo mouse event.
			 * First work out what modifiers are to be used.
			 */
			modifiers = 0x0;
			if (orig_mouse_code & MOUSE_SHIFT)
				modifiers |= MOD_MASK_SHIFT;
			if (orig_mouse_code & MOUSE_CTRL)
				modifiers |= MOD_MASK_CTRL;
			if (orig_mouse_code & MOUSE_ALT)
				modifiers |= MOD_MASK_ALT;
			if (orig_num_clicks == 2)
				modifiers |= MOD_MASK_2CLICK;
			else if (orig_num_clicks == 3)
				modifiers |= MOD_MASK_3CLICK;
			else if (orig_num_clicks == 4)
				modifiers |= MOD_MASK_4CLICK;

			/* Add the modifier codes to our string */
			if (modifiers != 0)
			{
				string[new_slen++] = K_SPECIAL;
				string[new_slen++] = KS_MODIFIER;
				string[new_slen++] = modifiers;
			}

			/* Work out our pseudo mouse event */
			key_name[0] = KS_EXTRA;
			key_name[1] = get_pseudo_mouse_code(current_button,
														   is_click, is_drag);
		}
#endif /* USE_MOUSE */
#ifdef USE_GUI
		/*
		 * If using the GUI, then we get menu and scrollbar events.
		 * 
		 * A menu event is encoded as K_SPECIAL, KS_MENU, K_FILLER followed by
		 * four bytes which are to be taken as a pointer to the GuiMenu
		 * structure.
		 *
		 * A scrollbar event is K_SPECIAL, KS_SCROLLBAR, K_FILLER followed by
		 * one byte representing the scrollbar number, and then four bytes
		 * representing a long_u which is the new value of the scrollbar.
		 *
		 * A horizontal scrollbar event is K_SPECIAL, KS_HORIZ_SCROLLBAR,
		 * K_FILLER followed by four bytes representing a long_u which is the
		 * new value of the scrollbar.
		 */
		else if (key_name[0] == KS_MENU)
		{
			num_bytes = get_long_from_buf(tp + slen, &val);
			if (num_bytes == -1)
				return -1;
			current_menu = (GuiMenu *)val;
			slen += num_bytes;
		}
		else if (key_name[0] == KS_SCROLLBAR)
		{
			num_bytes = get_bytes_from_buf(tp + slen, bytes, 1);
			if (num_bytes == -1)
				return -1;
			current_scrollbar = (int)bytes[0];
			slen += num_bytes;
			num_bytes = get_long_from_buf(tp + slen, &val);
			if (num_bytes == -1)
				return -1;
			scrollbar_value = val;
			slen += num_bytes;
		}
		else if (key_name[0] == KS_HORIZ_SCROLLBAR)
		{
			num_bytes = get_long_from_buf(tp + slen, &val);
			if (num_bytes == -1)
				return -1;
			scrollbar_value = val;
			slen += num_bytes;
		}
#endif /* USE_GUI */
		/* Finally, add the special key code to our string */
		string[new_slen++] = K_SPECIAL;
		string[new_slen++] = key_name[0];
		string[new_slen++] = key_name[1];
		string[new_slen] = NUL;
		extra = new_slen - slen;
		if (extra < 0)
				/* remove matched chars, taking care of noremap */
			del_typebuf(-extra, offset);
		else if (extra > 0)
				/* insert the extra space we need */
			ins_typebuf(string + slen, FALSE, offset, FALSE);

		/*
		 * Careful: del_typebuf() and ins_typebuf() may have
		 * reallocated typebuf[]
		 */
		vim_memmove(typebuf + typeoff + offset, string, (size_t)new_slen);
		return (len + extra + offset);
	}
	return 0;						/* no match found */
}

/*
 * Replace any terminal code strings in from[] with the equivalent internal
 * vim representation.	This is used for the "from" and "to" part of a
 * mapping, and the "to" part of a menu command.
 * Any strings like "<C_UP>" are also replaced, unless 'cpoptions' contains
 * '<'.  Also unshifts shifted special keys.
 * K_SPECIAL by itself is replaced by K_SPECIAL KS_SPECIAL K_FILLER.
 *
 * The replacement is done in result[] and finally copied into allocated
 * memory. If this all works well *bufp is set to the allocated memory and a
 * pointer to it is returned. If something fails *bufp is set to NULL and from
 * is returned.
 *
 * CTRL-V characters are removed.  When "from_part" is TRUE, a trailing CTRL-V
 * is included, otherwise it is removed (for ":map xx ^V", maps xx to
 * nothing).  When 'cpoptions' does not contain 'B', a backslash can be used
 * instead of a CTRL-V.
 */
	char_u	*
replace_termcodes(from, bufp, from_part)
	char_u	*from;
	char_u	**bufp;
	int		from_part;
{
	int		i;
	char_u	key_name[2];
	char_u	*bp;
	char_u	*last_dash;
	char_u	*end_of_name;
	int		slen;
	int		modifiers;
	int		bit;
	int		key;
	int		dlen = 0;
	char_u	*src;
	int		do_backslash;		/* backslash is a special character */
	int		do_special;			/* recognize <> key codes */
	int		do_key_code;		/* recognize raw key codes */
	char_u	*result;			/* buffer for resulting string */

	do_backslash = (vim_strchr(p_cpo, CPO_BSLASH) == NULL);
	do_special = (vim_strchr(p_cpo, CPO_SPECI) == NULL);
	do_key_code = (vim_strchr(p_cpo, CPO_KEYCODE) == NULL);

	/*
	 * Allocate space for the translation.  Worst case a single character is
	 * replaced by 6 bytes (shifted special key), plus a NUL at the end.
	 */
	result = alloc((unsigned)STRLEN(from) * 6 + 1);
	if (result == NULL)			/* out of memory */
	{
		*bufp = NULL;
		return from;
	}

	src = from;

	/*
	 * Check for #n at start only: function key n
	 */
	if (from_part && src[0] == '#' && isdigit(src[1]))		/* function key */
	{
		result[dlen++] = K_SPECIAL;
		result[dlen++] = 'k';
		if (src[1] == '0')
			result[dlen++] = ';';		/* #0 is F10 is "k;" */
		else
			result[dlen++] = src[1];	/* #3 is F3 is "k3" */
		src += 2;
	}

	/*
	 * Copy each byte from *from to result[dlen]
	 */
	while (*src != NUL)
	{
		/*
		 * If 'cpoptions' does not contain '<', check for special key codes.
		 */
		if (do_special)
		{
			/*
			 * See if it's a string like "<C-S-MouseLeft>"
			 */
			if (src[0] == '<')
			{
				/* Find end of modifier list */
				last_dash = src;
				for (bp = src + 1; *bp == '-' || isidchar(*bp); bp++)
				{
					if (*bp == '-')
					{
						last_dash = bp;
						if (bp[1] != NUL && bp[2] == '>')
							++bp;	/* anything accepted, like <C-?> */
					}
					if (bp[0] == 't' && bp[1] == '_' && bp[2] && bp[3])
						bp += 3;	/* skip t_xx, xx may be '-' or '>' */
				}

				if (*bp == '>')		/* found matching '>' */
				{
					end_of_name = bp + 1;

					/* Which modifiers are given? */
					modifiers = 0x0;
					for (bp = src + 1; bp < last_dash; bp++)
					{
						if (*bp != '-')
						{
							bit = name_to_mod_mask(*bp);
							if (bit == 0x0)
								break;		/* Illegal modifier name */
							modifiers |= bit;
						}
					}

					/*
					 * Legal modifier name.
					 */
					if (bp >= last_dash)
					{
						/*
						 * Modifier with single letter
						 */
						if (modifiers != 0 && last_dash[2] == '>')
						{
							key = last_dash[1];
							if (modifiers & MOD_MASK_SHIFT)
								key = TO_UPPER(key);
							if (modifiers & MOD_MASK_CTRL)
								key &= 0x1f;
							if (modifiers & MOD_MASK_ALT)
								key |= 0x80;
							src = end_of_name;
							result[dlen++] = key;
							continue;
						}

						/*
						 * Key name with or without modifier.
						 */
						else if ((key = get_special_key_code(last_dash + 1))
																		 != 0)
						{
							/* Put the appropriate modifier in a string */
							if (modifiers != 0)
							{
								result[dlen++] = K_SPECIAL;
								result[dlen++] = KS_MODIFIER;
								result[dlen++] = modifiers;
								/*
								 * Special trick: for <S-TAB>  K_TAB is used
								 * instead of TAB (there are two keys for the
								 * same thing).
								 */
								if (key == TAB)
									key = K_TAB;
							}

							if (IS_SPECIAL(key))
							{
								result[dlen++] = K_SPECIAL;
								result[dlen++] = KEY2TERMCAP0(key);
								result[dlen++] = KEY2TERMCAP1(key);
							}
							else
								result[dlen++] = key;		/* only modifiers */
							src = end_of_name;
							continue;
						}
					}
				}
			}
		}

		/*
		 * If 'cpoptions' does not contain 'k', see if it's an actual key-code.
		 * Note that this is also checked after replacing the <> form.
		 */
		if (do_key_code)
		{
			for (i = 0; i < tc_len; ++i)
			{
				slen = termcodes[i].len;
				if (STRNCMP(termcodes[i].code, src, (size_t)slen) == 0)
				{
					key_name[0] = termcodes[i].name[0];
					key_name[1] = termcodes[i].name[1];

					/*
					 * If it's a shifted special key, then include the SHIFT
					 * modifier
					 */
					if (unshift_special_key(&key_name[0]))
					{
						result[dlen++] = K_SPECIAL;
						result[dlen++] = KS_MODIFIER;
						result[dlen++] = MOD_MASK_SHIFT;
					}
					result[dlen++] = K_SPECIAL;
					result[dlen++] = key_name[0];
					result[dlen++] = key_name[1];
					src += slen;
					break;
				}
			}

			/*
			 * If terminal code matched, continue after it.
			 * If no terminal code matched and the character is K_SPECIAL,
			 * replace it with K_SPECIAL KS_SPECIAL K_FILLER
			 */
			if (i != tc_len)
				continue;
		}

		if (*src == K_SPECIAL)
		{
			result[dlen++] = K_SPECIAL;
			result[dlen++] = KS_SPECIAL;
			result[dlen++] = K_FILLER;
			++src;
			continue;
		}

		/*
		 * Remove CTRL-V and ignore the next character.
		 * For "from" side the CTRL-V at the end is included, for the "to"
		 * part it is removed.
		 * If 'cpoptions' does not contain 'B', also accept a backslash.
		 */
		key = *src;
		if (key == Ctrl('V') || (do_backslash && key == '\\'))
		{
			++src;								/* skip CTRL-V or backslash */
			if (*src == NUL)
			{
				if (from_part)
					result[dlen++] = key;
				break;
			}
		}
		result[dlen++] = *src++;
	}
	result[dlen] = NUL;

	/*
	 * Copy the new string to allocated memory.
	 * If this fails, just return from.
	 */
	if ((*bufp = strsave(result)) != NULL)
		from = *bufp;
	vim_free(result);
	return from;
}

/*
 * Gather the first characters in the terminal key codes into a string.
 * Used to speed up check_termcode().
 */
	static void
gather_termleader()
{
	int		i;
	int		len = 0;

#ifdef USE_GUI
	if (gui.in_use)
		termleader[len++] = CSI;	/* the GUI codes are not in termcodes[] */
#endif
	termleader[len] = NUL;

	for (i = 0; i < tc_len; ++i)
		if (vim_strchr(termleader, termcodes[i].code[0]) == NULL)
		{
			termleader[len++] = termcodes[i].code[0];
			termleader[len] = NUL;
		}

	need_gather = FALSE;
}

/*
 * Show all termcodes (for ":set termcap")
 * This code looks a lot like showoptions(), but is different.
 */
	void
show_termcodes()
{
	int				col;
	int				*items;
	int				item_count;
	int				run;
	int				row, rows;
	int				cols;
	int				i;
	int				len;

#define INC	27		/* try to make three columns */
#define GAP 2		/* spaces between columns */

	if (tc_len == 0)		/* no terminal codes (must be GUI) */
		return;
	items = (int *)alloc((unsigned)(sizeof(int) * tc_len));
	if (items == NULL)
		return;

	set_highlight('t');		/* Highlight title */
	start_highlight();
	MSG_OUTSTR("\n--- Terminal keys ---");
	stop_highlight();

	/*
	 * do the loop two times:
	 * 1. display the short items (non-strings and short strings)
	 * 2. display the long items (strings)
	 */
	for (run = 1; run <= 2 && !got_int; ++run)
	{
		/*
		 * collect the items in items[]
		 */
		item_count = 0;
		for (i = 0; i < tc_len; i++)
		{
			len = show_one_termcode(termcodes[i].name,
													termcodes[i].code, FALSE);
			if ((len <= INC - GAP && run == 1) || (len > INC - GAP && run == 2))
				items[item_count++] = i;
		}

		/*
		 * display the items
		 */
		if (run == 1)
		{
			cols = (Columns + GAP) / INC;
			if (cols == 0)
				cols = 1;
			rows = (item_count + cols - 1) / cols;
		}
		else	/* run == 2 */
			rows = item_count;
		for (row = 0; row < rows && !got_int; ++row)
		{
			msg_outchar('\n');						/* go to next line */
			if (got_int)							/* 'q' typed in more */
				break;
			col = 0;
			for (i = row; i < item_count; i += rows)
			{
				msg_pos(-1, col);					/* make columns */
				show_one_termcode(termcodes[items[i]].name,
											  termcodes[items[i]].code, TRUE);
				col += INC;
			}
			flushbuf();
			mch_breakcheck();
		}
	}
	vim_free(items);
}

/*
 * Show one termcode entry.
 * Output goes into IObuff[]
 */
	int
show_one_termcode(name, code, printit)
	char_u	*name;
	char_u	*code;
	int		printit;
{
	char_u		*p;
	int			len;

	if (name[0] > '~')
	{
		IObuff[0] = ' ';
		IObuff[1] = ' ';
		IObuff[2] = ' ';
		IObuff[3] = ' ';
	}
	else
	{
		IObuff[0] = 't';
		IObuff[1] = '_';
		IObuff[2] = name[0];
		IObuff[3] = name[1];
	}
	IObuff[4] = ' ';

	p = get_special_key_name(TERMCAP2KEY(name[0], name[1]), 0);
	if (p[1] != 't')
		STRCPY(IObuff + 5, p);
	else
		IObuff[5] = NUL;
	len = STRLEN(IObuff);
	do
		IObuff[len++] = ' ';
	while (len < 17);
	IObuff[len] = NUL;
	if (code == NULL)
		len += 4;
	else
		len += strsize(code);

	if (printit)
	{
		msg_outstr(IObuff);
		if (code == NULL)
			msg_outstr((char_u *)"NULL");
		else
			msg_outtrans(code);
	}
	return len;
}
