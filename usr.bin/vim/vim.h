/*	$OpenBSD: vim.h,v 1.2 1996/09/21 06:23:27 downsj Exp $	*/
/* vi:set ts=4 sw=4:
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */

/* ============ the header file puzzle (ca. 50-100 pieces) ========= */

#ifdef HAVE_CONFIG_H	/* GNU autoconf (or something else) was here */
# include "config.h"
#endif

#ifdef __EMX__			/* hand-edited config.h for OS/2 with EMX */
# include "conf_os2.h"
#endif

/*
 * This is a bit of a wishlist.  Currently we only have the Motif and Athena
 * GUI.
 */
#if defined(USE_GUI_MOTIF) \
	|| defined(USE_GUI_ATHENA) \
	|| defined(USE_GUI_MAC) \
	|| defined(USE_GUI_WINDOWS31) \
	|| defined(USE_GUI_WIN32) \
	|| defined(USE_GUI_OS2)
# ifndef USE_GUI
#  define USE_GUI
# endif
#endif

#include "feature.h"	/* #defines for optionals and features */

/*
 * Find out if function definitions should include argument types
 */
#ifdef AZTEC_C
# include <functions.h>
# define __ARGS(x)	x
# define __PARMS(x)	x
#endif

#ifdef SASC
# include <clib/exec_protos.h>
# define __ARGS(x)	x
# define __PARMS(x)	x
#endif

#ifdef _DCC
# include <clib/exec_protos.h>
# define __ARGS(x)	x
# define __PARMS(x)	x
#endif

#ifdef __TURBOC__
# define __ARGS(x) x
#endif

#if defined(UNIX) || defined(__EMX__)
# include "unix.h"		/* bring lots of system header files */
#endif

#ifdef VMS
# include "vms.h"
#endif

#ifndef __ARGS
# if defined(__STDC__) || defined(__GNUC__) || defined(WIN32)
#  define __ARGS(x) x
# else
#  define __ARGS(x) ()
# endif
#endif

/* __ARGS and __PARMS are the same thing. */
#ifndef __PARMS
# define __PARMS(x) __ARGS(x)
#endif

#ifdef UNIX
# include "osdef.h"		/* bring missing declarations in */
#endif

#ifdef __EMX__
# define	getcwd	_getcwd2
# define	chdir	_chdir2
# undef		CHECK_INODE
#endif

#ifdef AMIGA
# include "amiga.h"
#endif

#ifdef ARCHIE
# include "archie.h"
#endif

#ifdef MSDOS
# include "msdos.h"
#endif

#ifdef WIN32
# include "win32.h"
#endif

#ifdef MINT
# include "mint.h"
#endif

/*
 * Maximum length of a path	(for non-unix systems) Make it a bit long, to stay
 * on the safe side.  But not too long to put on the stack.
 */
#ifndef MAXPATHL
# ifdef MAXPATHLEN
#  define MAXPATHL	MAXPATHLEN
# else
#  define MAXPATHL	256
# endif
#endif

/*
 * Shorthand for unsigned variables. Many systems, but not all, have u_char
 * already defined, so we use char_u to avoid trouble.
 */
typedef unsigned char	char_u;
typedef unsigned short	short_u;
typedef unsigned int	int_u;
typedef unsigned long	long_u;

#ifndef UNIX				/* For Unix this is included in unix.h */
#include <stdio.h>
#include <ctype.h>
#endif

#if defined(HAVE_STRING_H)
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif

#include "ascii.h"
#include "keymap.h"
#include "term.h"
#include "macros.h"

#ifdef LATTICE
# include <sys/types.h>
# include <sys/stat.h>
#endif
#ifdef _DCC
# include <sys/stat.h>
#endif
#if defined MSDOS  ||  defined WIN32
# include <sys\stat.h>
#endif

/* allow other (non-unix) systems to configure themselves now */
#ifndef UNIX
# ifdef HAVE_STAT_H
#  include <stat.h>
# endif
# ifdef HAVE_STDLIB_H
#  include <stdlib.h>
# endif
#endif /* NON-UNIX */

/* ================ end of the header file puzzle =============== */

/*
 * flags for updateScreen()
 * The higher the value, the higher the priority
 */
#define VALID					10	/* buffer not changed */
#define INVERTED				20	/* redisplay inverted part */
#define VALID_TO_CURSCHAR		30	/* buffer at/below cursor changed */
#define NOT_VALID				40	/* buffer changed */
#define CURSUPD					50	/* buffer changed, update cursor first */
#define CLEAR					60	/* screen messed up, clear it */

/*
 * Attributes for NextScreen.
 */
#define CHAR_NORMAL		0
#define CHAR_INVERT		1
#define CHAR_UNDERL		2
#define CHAR_BOLD		3
#define CHAR_STDOUT		4
#define CHAR_ITALIC		5

/*
 * values for State
 *
 * The lowest four bits are used to distinguish normal/visual/cmdline/
 * insert+replace mode. This is used for mapping. If none of these bits are
 * set, no mapping is done.
 * The upper four bits are used to distinguish between other states.
 */
#define NORMAL					0x01
#define VISUAL					0x02
#define CMDLINE 				0x04
#define INSERT					0x08
#define NORMAL_BUSY				0x11	/* busy interpreting a command */
#define REPLACE 				0x28	/* replace mode */
#define HITRETURN				0x61	/* waiting for a return */
#define ASKMORE					0x70	/* Asking if you want --more-- */
#define SETWSIZE				0x80	/* window size has changed */
#define ABBREV					0x90	/* abbreviation instead of mapping */
#define EXTERNCMD				0xa0	/* executing an external command */

/* directions */
#define FORWARD 				1
#define BACKWARD				(-1)
#define BOTH_DIRECTIONS			2

/* return values for functions */
#define OK						1
#define FAIL					0

/*
 * values for command line completion
 */
#define CONTEXT_UNKNOWN			(-2)
#define EXPAND_UNSUCCESSFUL		(-1)
#define EXPAND_NOTHING			0
#define EXPAND_COMMANDS			1
#define EXPAND_FILES			2
#define EXPAND_DIRECTORIES		3
#define EXPAND_SETTINGS			4
#define EXPAND_BOOL_SETTINGS	5
#define EXPAND_TAGS				6
#define EXPAND_OLD_SETTING		7
#define EXPAND_HELP				8
#define EXPAND_BUFFERS			9
#define EXPAND_EVENTS			10
#define EXPAND_MENUS			11

/* Values for nextwild() and ExpandOne().  See ExpandOne() for meaning. */
#define WILD_FREE				1
#define WILD_EXPAND_FREE		2
#define WILD_EXPAND_KEEP		3
#define WILD_NEXT				4
#define WILD_PREV				5
#define WILD_ALL				6
#define WILD_LONGEST			7

#define WILD_LIST_NOTFOUND		1
#define WILD_HOME_REPLACE		2

/* Values for the find_pattern_in_path() function args 'type' and 'action': */
#define FIND_ANY		1
#define FIND_DEFINE		2
#define CHECK_PATH		3

#define ACTION_SHOW		1
#define ACTION_GOTO		2
#define ACTION_SPLIT	3
#define ACTION_SHOW_ALL	4
#ifdef INSERT_EXPAND
# define ACTION_EXPAND	5
#endif

/* Values for 'options' argument in do_search() and searchit() */
#define SEARCH_REV	  0x01	/* go in reverse of previous dir. */
#define SEARCH_ECHO	  0x02	/* echo the search command and handle options */
#define SEARCH_MSG	  0x0c	/* give messages (yes, it's not 0x04) */
#define SEARCH_NFMSG  0x08	/* give all messages except not found */
#define SEARCH_OPT	  0x10	/* interpret optional flags */
#define SEARCH_HIS	  0x20	/* put search pattern in history */
#define SEARCH_END	  0x40	/* put cursor at end of match */
#define SEARCH_NOOF	  0x80	/* don't add offset to position */
#define SEARCH_START 0x100	/* start search without col offset */
#define SEARCH_MARK  0x200	/* set previous context mark */
#define SEARCH_KEEP  0x400	/* keep previous search pattern */

/* Values for find_ident_under_cursor() */
#define FIND_IDENT	1		/* find identifier (word) */
#define FIND_STRING	2		/* find any string (WORD) */

/* Values for get_file_name_in_path() */
#define FNAME_MESS	1		/* give error message */
#define FNAME_EXP	2		/* expand to path */
#define FNAME_HYP	4		/* check for hypertext link */

/* Values for buflist_getfile() */
#define GETF_SETMARK	0x01	/* set pcmark before jumping */
#define GETF_ALT		0x02	/* jumping to alternate file (not buf num) */

/* Values for in_indentkeys() */
#define KEY_OPEN_FORW	0x101
#define KEY_OPEN_BACK	0x102

/* Values for call_shell() second argument */
#define SHELL_FILTER		1		/* filtering text */
#define SHELL_EXPAND		2		/* expanding wildcards */
#define SHELL_COOKED		4		/* set term to cooked mode */

/* Values for change_indent() */
#define INDENT_SET			1		/* set indent */
#define INDENT_INC			2		/* increase indent */
#define INDENT_DEC			3		/* decrease indent */

/* Values for flags argument for findmatchlimit() */
#define FM_BACKWARD			0x01	/* search backwards */
#define FM_FORWARD			0x02	/* search forwards */
#define FM_BLOCKSTOP		0x04	/* stop at start/end of block */
#define FM_SKIPCOMM			0x08	/* skip comments */

/* Values for action argument for do_buffer() */
#define DOBUF_GOTO		0		/* go to specified buffer */
#define DOBUF_SPLIT		1		/* split window and go to specified buffer */
#define DOBUF_UNLOAD	2		/* unload specified buffer(s) */
#define DOBUF_DEL		3		/* delete specified buffer(s) */

/* Values for start argument for do_buffer() */
#define DOBUF_CURRENT	0		/* "count" buffer from current buffer */
#define DOBUF_FIRST		1		/* "count" buffer from first buffer */
#define DOBUF_LAST		2		/* "count" buffer from last buffer */
#define DOBUF_MOD		3		/* "count" mod. buffer from current buffer */

/* Values for sub_cmd and which_pat argument for myregcomp() */
/* Also used for which_pat argument for searchit() */
#define RE_SEARCH	0			/* save/use pat in/from search_pattern */
#define RE_SUBST	1			/* save/use pat in/from subst_pattern */
#define RE_BOTH		2			/* save pat in both patterns */
#define RE_LAST		2			/* use last used pattern if "pat" is NULL */

/* Return values for fullpathcmp() */
#define FPC_SAME   1			/* both exist and are the same file. */
#define FPC_DIFF   2			/* both exist and are different files. */
#define FPC_NOTX   3			/* both don't exist. */
#define FPC_DIFFX  4			/* one of them doesn't exist. */

/* flags for do_ecmd() */
#define ECMD_HIDE		1		/* don't free the current buffer */
#define ECMD_SET_HELP	2		/* set b_help flag of (new) buffer before
								   opening file */
#define ECMD_OLDBUF		4		/* use existing buffer if it exists */
#define ECMD_FORCEIT	8		/* ! used in Ex command */

/*
 * Events for autocommands.
 */
enum auto_events
{
	EVENT_BUFENTER = 0,		/* after entering a buffer */
	EVENT_BUFLEAVE,			/* before leaving a buffer */
	EVENT_BUFNEWFILE,		/* when creating a buffer for a new file */
	EVENT_BUFREADPOST,		/* after reading a buffer */
	EVENT_BUFREADPRE,		/* before reading a buffer */
	EVENT_BUFWRITEPOST,		/* after writing a buffer */
	EVENT_BUFWRITEPRE,		/* before writing a buffer */
	EVENT_FILEAPPENDPOST,	/* after appending to a file */
	EVENT_FILEAPPENDPRE,	/* before appending to a file */
	EVENT_FILEREADPOST,		/* after reading a file */
	EVENT_FILEREADPRE,		/* before reading a file */
	EVENT_FILEWRITEPOST,	/* after writing a file */
	EVENT_FILEWRITEPRE,		/* before writing a file */
	EVENT_FILTERREADPOST,	/* after reading from a filter */
	EVENT_FILTERREADPRE,	/* before reading from a filter */
	EVENT_FILTERWRITEPOST,	/* after writing to a filter */
	EVENT_FILTERWRITEPRE,	/* before writing to a filter */
	EVENT_VIMLEAVE,			/* before exiting Vim */
	EVENT_WINENTER,			/* after entering a window */
	EVENT_WINLEAVE,			/* before leaving a window */
	NUM_EVENTS				/* MUST be the last one */
};

/*
 * Boolean constants
 */
#ifndef TRUE
# define FALSE	0			/* note: this is an int, not a long! */
# define TRUE	1
#endif

#define MAYBE	2			/* for beginline() and the 'sol' option */

/* May be returned by add_new_completion(): */
#define RET_ERROR				(-1)

/*
 * jump_to_mouse() returns one of these values, possibly with
 * CURSOR_MOVED added
 */
#define IN_UNKNOWN		1
#define IN_BUFFER		2
#define IN_STATUS_LINE	3			/* Or in command line */
#define CURSOR_MOVED	0x100

/* flags for jump_to_mouse() */
#define MOUSE_FOCUS		0x1		/* if used, need to stay in this window */
#define MOUSE_MAY_VIS	0x2		/* if used, may set visual mode */ 
#define MOUSE_DID_MOVE	0x4		/* if used, only act when mouse has moved */
#define MOUSE_SETPOS	0x8		/* if used, only set current mouse position */

/*
 * Minimum screen size
 */
#define MIN_COLUMNS		12		/* minimal columns for screen */
#define MIN_ROWS		1		/* minimal rows for one window */
#define STATUS_HEIGHT	1		/* height of a status line under a window */

/*
 * Buffer sizes
 */
#ifndef CMDBUFFSIZE
# define CMDBUFFSIZE	256		/* size of the command processing buffer */
#endif

#define LSIZE		512			/* max. size of a line in the tags file */

#define IOSIZE	   (1024+1) 	/* file i/o and sprintf buffer size */
#define MSG_BUF_LEN	80			/* length of buffer for small messages */

#define	TERMBUFSIZE	1024

#if defined(AMIGA) || defined(__linux__)
# define TBUFSZ 2048			/* buffer size for termcap entry */
#else
# define TBUFSZ 1024			/* buffer size for termcap entry */
#endif

/*
 * Maximum length of key sequence to be mapped.
 * Must be able to hold an Amiga resize report.
 */
#define MAXMAPLEN	50

#ifdef BINARY_FILE_IO
# define WRITEBIN	"wb"		/* no CR-LF translation */
# define READBIN	"rb"
# define APPENDBIN	"ab"
#else
# define WRITEBIN	"w"
# define READBIN	"r"
# define APPENDBIN	"a"
#endif

/*
 * EMX doesn't have a global way of making open() use binary I/O.
 * Use O_BINARY for all open() calls.
 */
#ifdef __EMX__
# define O_EXTRA	O_BINARY
#else
# define O_EXTRA	0
#endif

#define CHANGED   		set_Changed()
#define UNCHANGED(buf)	unset_Changed(buf)

/*
 * defines to avoid typecasts from (char_u *) to (char *) and back
 * (vim_strchr() and vim_strrchr() are now in alloc.c)
 */
#define STRLEN(s)			strlen((char *)(s))
#define STRCPY(d, s)		strcpy((char *)(d), (char *)(s))
#define STRNCPY(d, s, n)	strncpy((char *)(d), (char *)(s), (size_t)(n))
#define STRCMP(d, s)		strcmp((char *)(d), (char *)(s))
#define STRNCMP(d, s, n)	strncmp((char *)(d), (char *)(s), (size_t)(n))
#define STRCAT(d, s)		strcat((char *)(d), (char *)(s))
#define STRNCAT(d, s, n)	strncat((char *)(d), (char *)(s), (size_t)(n))

#define MSG(s)				msg((char_u *)(s))
#define EMSG(s)				emsg((char_u *)(s))
#define EMSG2(s, p)			emsg2((char_u *)(s), (char_u *)(p))
#define EMSGN(s, n)			emsgn((char_u *)(s), (long)(n))
#define OUTSTR(s)			outstr((char_u *)(s))
#define OUTSTRN(s)			outstrn((char_u *)(s))
#define MSG_OUTSTR(s)		msg_outstr((char_u *)(s))

typedef long		linenr_t;		/* line number type */
typedef unsigned	colnr_t;		/* column number type */

#define MAXLNUM (0x7fffffff)		/* maximum (invalid) line number */

#if SIZEOF_INT >= 4
# define MAXCOL	(0x7fffffff)		/* maximum column number, 31 bits */
#else
# define MAXCOL	(0x7fff)			/* maximum column number, 15 bits */
#endif

#define SHOWCMD_COLS 10				/* columns needed by shown command */

/*
 * Include a prototype for vim_memmove(), it may not be in alloc.pro.
 */
#ifdef VIM_MEMMOVE
void vim_memmove __ARGS((void *, void *, size_t));
#else
# ifndef vim_memmove
#  define vim_memmove(to, from, len) memmove(to, from, len)
# endif
#endif

/*
 * For the Amiga we use a version of getenv that does local variables under 2.0
 * For Win32 and MSDOS we also check $HOME when $VIM is used.
 */
#if !defined(AMIGA) && !defined(WIN32) && !defined(MSDOS) && !defined(VMS)
# define vim_getenv(x) (char_u *)getenv((char *)x)
#endif

/*
 * fnamecmp() is used to compare filenames.
 * On some systems case in a filename does not matter, on others it does.
 * (this does not account for maximum name lengths and things like "../dir",
 * thus it is not 100% accurate!)
 */
#ifdef CASE_INSENSITIVE_FILENAME
# define fnamecmp(x, y) stricmp((char *)(x), (char *)(y))
# define fnamencmp(x, y, n) strnicmp((char *)(x), (char *)(y), (size_t)(n))
#else
# define fnamecmp(x, y) strcmp((char *)(x), (char *)(y))
# define fnamencmp(x, y, n) strncmp((char *)(x), (char *)(y), (size_t)(n))
#endif

#ifdef HAVE_MEMSET
# define vim_memset(ptr, c, size)	memset((ptr), (c), (size))
#else
void *vim_memset __ARGS((void *, int, size_t));
#endif

/* for MS-DOS and Win32: use chdir() that also changes the default drive */
#ifdef USE_VIM_CHDIR
int vim_chdir __ARGS((char *));
#else
# define vim_chdir chdir
#endif

/*
 * vim_iswhite() is used for "^" and the like. It differs from isspace()
 * because it doesn't include <CR> and <LF> and the like.
 */
#define vim_iswhite(x)	((x) == ' ' || (x) == '\t')

/* Note that gui.h is included by structs.h */

#include "structs.h"		/* file that defines many structures */
