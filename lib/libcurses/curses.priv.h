
/***************************************************************************
*                            COPYRIGHT NOTICE                              *
****************************************************************************
*                ncurses is copyright (C) 1992-1995                        *
*                          Zeyd M. Ben-Halim                               *
*                          zmbenhal@netcom.com                             *
*                          Eric S. Raymond                                 *
*                          esr@snark.thyrsus.com                           *
*                                                                          *
*        Permission is hereby granted to reproduce and distribute ncurses  *
*        by any means and for any fee, whether alone or as part of a       *
*        larger distribution, in source or in binary form, PROVIDED        *
*        this notice is included with any such distribution, and is not    *
*        removed from any of its header files. Mention of ncurses in any   *
*        applications linked with it is highly appreciated.                *
*                                                                          *
*        ncurses comes AS IS with no warranty, implied or expressed.       *
*                                                                          *
***************************************************************************/


/*
 * Id: curses.priv.h,v 1.61 1997/04/26 20:54:54 tom Exp $
 *
 *	curses.priv.h
 *
 *	Header file for curses library objects which are private to
 *	the library.
 *
 */

#ifndef CURSES_PRIV_H
#define CURSES_PRIV_H 1

#include <ncurses_cfg.h>

#ifdef USE_RCS_IDS
#define MODULE_ID(id) static const char Ident[] = id;
#else
#define MODULE_ID(id) /*nothing*/
#endif

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#if HAVE_SYS_BSDTYPES_H
#include <sys/bsdtypes.h>	/* needed for ISC */
#endif

#if HAVE_LIMITS_H
# include <limits.h>
#elif HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif

#ifndef PATH_MAX
# if defined(_POSIX_PATH_MAX)
#  define PATH_MAX _POSIX_PATH_MAX
# elif defined(MAXPATHLEN)
#  define PATH_MAX MAXPATHLEN
# else
#  define PATH_MAX 255	/* the Posix minimum path-size */
# endif
#endif

#include <assert.h>
#include <stdio.h>

#include <errno.h>

#if !HAVE_EXTERN_ERRNO
extern int errno;
#endif

/* Some systems have a broken 'select()', but workable 'poll()'.  Use that */
#if HAVE_POLL && HAVE_SYS_STROPTS_H && HAVE_POLL_H
#define USE_FUNC_POLL 1
#else
#define USE_FUNC_POLL 0
#endif

/* Alessandro Rubini's GPM (general-purpose mouse) */
#if HAVE_LIBGPM && HAVE_GPM_H
#define USE_GPM_SUPPORT 1
#else
#define USE_GPM_SUPPORT 0
#endif

/*
 * ht/cbt expansion flakes out randomly under Linux 1.1.47, but only when
 * we're throwing control codes at the screen at high volume.  To see this,
 * re-enable TABS_OK and run worm for a while.  Other systems probably don't
 * want to define this either due to uncertainties about tab delays and
 * expansion in raw mode.
 */
#undef TABS_OK	/* OK to use tab/backtab for local motions? */

/*
 * The internal types (e.g., struct screen) must precede <curses.h>, otherwise
 * we cannot construct lint-libraries (structures must be fully-defined).
 */

struct tries {
	struct tries    *child;     /* ptr to child.  NULL if none          */
	struct tries    *sibling;   /* ptr to sibling.  NULL if none        */
	unsigned char    ch;        /* character at this node               */
	unsigned short   value;     /* code of string so far.  0 if none.   */
};

/*
 * Structure for soft labels.
 */

typedef struct
{
	char *text;             /* text for the label */
	char *form_text;        /* formatted text (left/center/...) */
	int x;                  /* x coordinate of this field */
	char dirty;             /* this label has changed */
	char visible;           /* field is visible */
} slk_ent;

typedef struct {
	char dirty;             /* all labels have changed */
	char hidden;            /* soft labels are hidden */
	struct _win_st *win;
	slk_ent *ent;
	char *buffer;           /* buffer for labels */
	short maxlab;           /* number of available labels */
	short labcnt;           /* number of allocated labels */
	short maxlen;           /* length of labels */
} SLK;

/*
 * Definitions for color pairs
 */
#define C_SHIFT 8		/* we need more bits than there are colors */
#define C_MASK  ((1 << C_SHIFT) - 1)

#define PAIR_OF(fg, bg) ((((fg) & C_MASK) << C_SHIFT) | ((bg) & C_MASK))

/*
 * Structure for palette tables
 */

typedef struct
{
    short red, green, blue;
}
color_t;

#define MAXCOLUMNS    135
#define MAXLINES      66
#define FIFO_SIZE     MAXLINES

struct screen {
	int             _ifd;           /* input file ptr for screen        */
	FILE            *_ofp;          /* output file ptr for screen       */
	char            *_setbuf;       /* buffered I/O for output          */
	int             _checkfd;       /* filedesc for typeahead check     */
#ifdef EXTERN_TERMINFO
	struct _terminal *_term;         /* terminal type information        */
#else
	struct term     *_term;         /* terminal type information        */
#endif
	short           _lines;         /* screen lines                     */
	short           _columns;       /* screen columns                   */
	short           _lines_avail;   /* lines available for stdscr       */
	short           _topstolen;     /* lines stolen from top            */
	struct _win_st  *_curscr;       /* current screen                   */
	struct _win_st  *_newscr;       /* virtual screen to be updated to  */
	struct _win_st  *_stdscr;       /* screen's full-window context     */
	struct tries    *_keytry;       /* "Try" for use with keypad mode   */
	unsigned int    _fifo[FIFO_SIZE];       /* input push-back buffer   */
	signed char     _fifohead,      /* head of fifo queue               */
	                _fifotail,      /* tail of fifo queue               */
	                _fifopeek;      /* where to peek for next char      */
	int             _endwin;        /* are we out of window mode?       */
	unsigned long   _current_attr;  /* terminal attribute current set   */
	int             _coloron;       /* is color enabled?                */
	int             _cursor;        /* visibility of the cursor         */
	int             _cursrow;       /* physical cursor row              */
	int             _curscol;       /* physical cursor column           */
	int             _nl;            /* True if NL -> CR/NL is on        */
	int             _raw;           /* True if in raw mode              */
	int             _cbreak;        /* 1 if in cbreak mode              */
	                                /* > 1 if in halfdelay mode         */
	int             _echo;          /* True if echo on                  */
	int             _use_meta;      /* use the meta key?                */
	SLK             *_slk;          /* ptr to soft key struct / NULL    */
	int             _baudrate;      /* used to compute padding          */

	/* cursor movement costs; units are 10ths of milliseconds */
	int             _char_padding;  /* cost of character put            */
	int             _cr_cost;       /* cost of (carriage_return)        */
	int             _cup_cost;      /* cost of (cursor_address)         */
	int             _home_cost;     /* cost of (cursor_home)            */
	int             _ll_cost;       /* cost of (cursor_to_ll)           */
#ifdef TABS_OK
	int             _ht_cost;       /* cost of (tab)                    */
	int             _cbt_cost;      /* cost of (backtab)                */
#endif /* TABS_OK */
	int             _cub1_cost;     /* cost of (cursor_left)            */
	int             _cuf1_cost;     /* cost of (cursor_right)           */
	int             _cud1_cost;     /* cost of (cursor_down)            */
	int             _cuu1_cost;     /* cost of (cursor_up)              */
	int             _cub_cost;      /* cost of (parm_cursor_left)       */
	int             _cuf_cost;      /* cost of (parm_cursor_right)      */
	int             _cud_cost;      /* cost of (parm_cursor_down)       */
	int             _cuu_cost;      /* cost of (parm_cursor_up)         */
	int             _hpa_cost;      /* cost of (column_address)         */
	int             _vpa_cost;      /* cost of (row_address)            */
	/* used in lib_doupdate.c, must be chars */
	int             _ed_cost;       /* cost of (clr_eos)                */
	int             _el_cost;       /* cost of (clr_eol)                */
	int             _el1_cost;      /* cost of (clr_bol)                */
	int             _dch1_cost;     /* cost of (delete_character)       */
	int             _ich1_cost;     /* cost of (insert_character)       */
	int             _dch_cost;      /* cost of (parm_dch)               */
	int             _ich_cost;      /* cost of (parm_ich)               */
	int             _ech_cost;      /* cost of (erase_chars)            */
	int             _rep_cost;      /* cost of (repeat_char)            */
	int             _hpa_ch_cost;   /* cost of (column_address)         */
	int             _cup_ch_cost;   /* cost of (cursor_address)         */
	/* used in lib_mvcur.c */
	char *          _address_cursor;
	int             _carriage_return_length;
	int             _cursor_home_length;
	int             _cursor_to_ll_length;
	/* used in lib_color.c */
	color_t         *_color_table;  /* screen's color palette            */
	int             _color_count;   /* count of colors in palette        */
	unsigned short  *_color_pairs;  /* screen's color pair list          */
	int             _pair_count;    /* count of color pairs              */
	int             _default_color; /* use default colors                */
};

/* Ncurses' public interface follows the internal types */
#include <curses.h>	/* we'll use -Ipath directive to get the right one! */

#ifdef NCURSES_NOMACROS
#include <nomacros.h>
#endif

#define WINDOWLIST struct _win_list
	WINDOWLIST {
	WINDOWLIST *next;
	WINDOW	*win;
};

typedef	struct {
	int	line;                   /* lines to take, < 0 => from bottom*/
	int	(*hook)(struct _win_st *, int); /* callback for user        */
	struct _win_st *w;              /* maybe we need this for cleanup   */
} ripoff_t;

/* The terminfo source is assumed to be 7-bit ASCII */
#define is7bits(c)	((unsigned)(c) < 128)

#ifndef min
#define min(a,b)	((a) > (b)  ?  (b)  :  (a))
#endif

#ifndef max
#define max(a,b)	((a) < (b)  ?  (b)  :  (a))
#endif

/* usually in <unistd.h> */
#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif

#ifndef STDERR_FILENO
#define STDERR_FILENO 2
#endif

#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif

#ifndef EXIT_FAILURE
#define EXIT_FAILURE 1
#endif

#ifndef R_OK
#define	R_OK	4		/* Test for read permission.  */
#endif
#ifndef W_OK
#define	W_OK	2		/* Test for write permission.  */
#endif
#ifndef X_OK
#define	X_OK	1		/* Test for execute permission.  */
#endif
#ifndef F_OK
#define	F_OK	0		/* Test for existence.  */
#endif

#define TextOf(c)    ((c) & (chtype)A_CHARTEXT)
#define AttrOf(c)    ((c) & (chtype)A_ATTRIBUTES)

#define BLANK        (' '|A_NORMAL)

#define CHANGED     -1

#define typeCalloc(type,elts) (type *)calloc(elts,sizeof(type))
#define FreeIfNeeded(p)  if(p != 0) free(p)
#define FreeAndNull(p)   free(p); p = 0

#include <nc_alloc.h>

/*
 * Prefixes for call/return points of library function traces.  We use these to
 * instrument the public functions so that the traces can be easily transformed
 * into regression scripts.
 */
#define T_CALLED(fmt) "called " fmt
#define T_CREATE(fmt) "create " fmt
#define T_RETURN(fmt) "return " fmt

#ifdef TRACE
#define TR(n, a)	if (_nc_tracing & (n)) _tracef a
#define T(a)		TR(TRACE_CALLS, a)
#define TPUTS_TRACE(s)	_nc_tputs_trace = s;
#define TRACE_RETURN(value,type) return _nc_retrace_##type(value)
#define returnWin(code)  TRACE_RETURN(code,win)
#define returnPtr(code)  TRACE_RETURN(code,ptr)
#define returnCode(code) TRACE_RETURN(code,int)
#define returnVoid       T((T_RETURN(""))); return
extern unsigned _nc_tracing;
extern WINDOW * _nc_retrace_win(WINDOW *);
extern char *_nc_retrace_ptr(char *);
extern const char *_nc_tputs_trace;
extern const char *_nc_visbuf(const char *);
extern const char *_nc_visbuf2(int, const char *);
extern int _nc_retrace_int(int);
extern long _nc_outchars;
#else
#define T(a)
#define TR(n, a)
#define TPUTS_TRACE(s)
#define returnWin(code)  return code
#define returnCode(code) return code
#define returnVoid       return
#endif

#define _trace_key(ch) ((ch > KEY_MIN) ? keyname(ch) : _tracechar((unsigned char)ch))

#define ALL_BUT_COLOR ((chtype)~(A_COLOR))
#define IGNORE_COLOR_OFF FALSE


/* Macro to put together character and attribute info and return it.
   If colors are in the attribute, they have precedence. */
#define ch_or_attr(ch,at) \
    ((PAIR_NUMBER(at) > 0) ? \
     ((((chtype)ch) & ALL_BUT_COLOR) | (at)) : ((((chtype)ch) | (at))))


#define toggle_attr_on(S,at) \
   if (PAIR_NUMBER(at) > 0)\
      (S) = ((S) & ALL_BUT_COLOR) | (at);\
   else\
      (S) |= (at);\
   T(("new attribute is %s", _traceattr((S))))


#define toggle_attr_off(S,at) \
   if (IGNORE_COLOR_OFF == TRUE) {\
      if (PAIR_NUMBER(at) == 0xff) /* turn off color */\
	 (S) &= ~(at);\
      else /* leave color alone */\
	 (S) &= ~((at)&ALL_BUT_COLOR);\
   } else {\
      if (PAIR_NUMBER(at) > 0x00) /* turn off color */\
	 (S) &= ~(at|A_COLOR);\
      else /* leave color alone */\
	 (S) &= ~(at);\
   }\
   T(("new attribute is %s", _traceattr((S))));

#define DelCharCost(count) \
		((parm_dch != 0) \
		? SP->_dch_cost \
		: ((delete_character != 0) \
			? (SP->_dch1_cost * count) \
			: INFINITY))

#define InsCharCost(count) \
		((parm_ich != 0) \
		? SP->_ich_cost \
		: ((insert_character != 0) \
			? (SP->_ich1_cost * count) \
			: INFINITY))

#define UpdateAttrs(c)	if (SP->_current_attr != AttrOf(c)) \
				vidattr(AttrOf(c));

/*
 * Check whether the given character can be output by clearing commands.  This
 * includes test for being a space and not including any 'bad' attributes, such
 * as A_REVERSE.  All attribute flags which don't affect appearance of a space
 * or can be output by clearing (A_COLOR in case of bce-terminal) are excluded.
 */
#define NONBLANK_ATTR (A_BOLD|A_DIM|A_BLINK)
#define can_clear_with(ch) \
	((ch & ~(NONBLANK_ATTR|(back_color_erase ? A_COLOR:0))) == BLANK)

#ifdef NCURSES_EXPANDED

#undef  ch_or_attr
#define ch_or_attr(ch,at) _nc_ch_or_attr(ch,at)
extern chtype _nc_ch_or_attr(chtype, attr_t);

#undef  toggle_attr_on
#define toggle_attr_on(S,at) _nc_toggle_attr_on(&(S), at)
extern void _nc_toggle_attr_on(attr_t *, attr_t);

#undef  toggle_attr_off
#define toggle_attr_off(S,at) _nc_toggle_attr_off(&(S), at)
extern void _nc_toggle_attr_off(attr_t *, attr_t);

#undef  can_clear_with
#define can_clear_with(ch) _nc_can_clear_with(ch)
extern int _nc_can_clear_with(chtype);

#undef  DelCharCost
#define DelCharCost(count) _nc_DelCharCost(count)
extern int _nc_DelCharCost(int);

#undef  InsCharCost
#define InsCharCost(count) _nc_InsCharCost(count)
extern int _nc_InsCharCost(int);

#undef  UpdateAttrs
#define UpdateAttrs(c) _nc_UpdateAttrs(c)
extern void _nc_UpdateAttrs(chtype);

#else

extern void _nc_expanded(void);

#endif

/* lib_acs.c */
extern void init_acs(void);	/* no prefix, this name is traditional */

/* lib_mvcur.c */
#define INFINITY	1000000	/* cost: too high to use */

extern void _nc_mvcur_init(void);
extern void _nc_mvcur_resume(void);
extern void _nc_mvcur_wrap(void);
extern int _nc_mvcur_scrolln(int, int, int, int);

/* lib_mouse.c */
extern void _nc_mouse_init(SCREEN *);
extern bool _nc_mouse_event(SCREEN *);
extern bool _nc_mouse_inline(SCREEN *);
extern bool _nc_mouse_parse(int);
extern void _nc_mouse_wrap(SCREEN *);
extern void _nc_mouse_resume(SCREEN *);
extern int _nc_max_click_interval;
extern int _nc_mouse_fd(void);

/* elsewhere ... */
extern WINDOW *_nc_makenew(int, int, int, int, int);
extern chtype _nc_background(WINDOW *);
extern chtype _nc_render(WINDOW *, chtype);
extern char *_nc_trace_buf(int, size_t);
extern int _nc_initscr(void);
extern int _nc_keypad(bool);
#ifdef EXTERN_TERMINFO                                                      
#define	_nc_outch _ti_outc                                                  
#endif                                                                      
extern int _nc_outch(int);
extern int _nc_setupscreen(short, short const, FILE *);
extern int _nc_timed_wait(int, int, int *);
extern int _nc_waddch_nosync(WINDOW *, const chtype);
extern void _nc_backspace(WINDOW *win);
extern void _nc_do_color(int, int (*)(int));
extern void _nc_freeall(void);
extern void _nc_free_and_exit(int);
extern void _nc_freewin(WINDOW *win);
extern void _nc_get_screensize(void);
extern void _nc_hash_map(void);
extern void _nc_outstr(const char *str);
extern void _nc_scroll_optimize(void);
extern void _nc_scroll_window(WINDOW *, int const, short const, short const);
extern void _nc_set_buffer(FILE *ofp, bool buffered);
extern void _nc_signal_handler(bool);
extern void _nc_synchook(WINDOW *win);

/*
 * On systems with a broken linker, define 'SP' as a function to force the
 * linker to pull in the data-only module with 'SP'.
 */
#ifndef BROKEN_LINKER
#define BROKEN_LINKER 0
#endif

#if BROKEN_LINKER
#define SP _nc_screen()
extern SCREEN *_nc_screen(void);
extern int _nc_alloc_screen(void);
extern void _nc_set_screen(SCREEN *);
#else
extern SCREEN *SP;
#define _nc_alloc_screen() ((SP = typeCalloc(SCREEN, 1)) != 0)
#define _nc_set_screen(sp) SP = sp
#endif

#if !HAVE_USLEEP
extern int _nc_usleep(unsigned int);
#define usleep(msecs) _nc_usleep(msecs)
#endif

/*
 * ncurses' term.h defines this but since we use our own terminfo
 * we need to define it here.
 */
#ifdef EXTERN_TERMINFO                                                      
#define	GET_TTY(fd, buf) tcgetattr(fd, buf)
#define	SET_TTY(fd, buf) tcsetattr(fd, TCSADRAIN, buf)
#endif                                                                      

/*
 * We don't want to use the lines or columns capabilities internally,
 * because if the application is running multiple screens under
 * X windows, it's quite possible they could all have type xterm
 * but have different sizes!  So...
 */
#define screen_lines	SP->_lines
#define screen_columns	SP->_columns

extern int _nc_slk_format;  /* != 0 if slk_init() called */
extern int _nc_slk_initialize(WINDOW *, int);

/* Macro to check whether or not we use a standard format */
#define SLK_STDFMT (_nc_slk_format < 3)
/* Macro to determine height of label window */
#define SLK_LINES  (SLK_STDFMT ? 1 : (_nc_slk_format - 2))

extern int _nc_ripoffline(int line, int (*init)(WINDOW *,int));

#define UNINITIALISED ((struct tries * ) -1)

extern bool _nc_idlok, _nc_idcok;

extern WINDOWLIST *_nc_windows;

#endif /* CURSES_PRIV_H */
