
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
 *	curses.priv.h
 *
 *	Header file for curses library objects which are private to
 *	the library.
 *
 */

#include <config.h>

#if HAVE_UNISTD_H
#include <unistd.h>
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
#  define PATH_MAX 255	/* the Posix minimum pathsize */
# endif
#endif

#include <assert.h>

#include <curses.h>	/* we'll use -Ipath directive to get the right one! */

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

/*
 * ht/cbt expansion flakes out randomly under Linux 1.1.47, but only when
 * we're throwing control codes at the screen at high volume.  To see this, 
 * re-enable TABS_OK and run worm for a while.  Other systems probably don't
 * want to define this either due to uncertainties about tab delays and
 * expansion in raw mode. 
 */
#undef TABS_OK	/* OK to use tab/backtab for local motions? */

#ifdef TRACE
#define T(a)	if (_nc_tracing & TRACE_CALLS) _tracef a 
#define TR(n, a)	if (_nc_tracing & (n)) _tracef a 
#define TPUTS_TRACE(s)	_nc_tputs_trace = s;
extern unsigned _nc_tracing;
extern char *_nc_tputs_trace;
extern char *_nc_visbuf(const char *);
#else	
#define T(a)
#define TR(n, a)
#define TPUTS_TRACE(s)
#endif

/* lib_acs.c */
extern void init_acs(void);	/* no prefix, this name is traditional */

/* lib_mvcur.c */
extern void _nc_mvcur_init(SCREEN *sp);
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

/* elsewhere ... */
extern int _nc_keypad(bool flag);
extern WINDOW *_nc_makenew(int, int, int, int);
#ifdef EXTERN_TERMINFO
#define _nc_outch _ti_outc
#endif
extern int _nc_outch(int);
extern chtype _nc_render(WINDOW *, chtype, chtype);
extern int _nc_waddch_nosync(WINDOW *, const chtype);
extern void _nc_scroll_optimize(void);
extern void _nc_scroll_window(WINDOW *, int const, short const, short const);
extern int _nc_setupscreen(short, short const);
extern void _nc_backspace(WINDOW *win);
extern void _nc_outstr(char *str);
extern void _nc_signal_handler(bool);
extern void _nc_synchook(WINDOW *win);
extern int _nc_timed_wait(int fd, int wait, int *timeleft);
extern void _nc_do_color(int, int (*)(int));

struct try {
        struct try      *child;     /* ptr to child.  NULL if none          */
        struct try      *sibling;   /* ptr to sibling.  NULL if none        */
        unsigned char    ch;        /* character at this node               */
        unsigned short   value;     /* code of string so far.  0 if none.   */
};
  
/*
 * Structure for soft labels.
 */
  
typedef struct {
	char dirty;			/* all labels have changed */
	char hidden;			/* soft lables are hidden */
	WINDOW *win;
 	struct slk_ent {
 	    char text[9];		/* text for the label */
 	    char form_text[9];		/* formatted text (left/center/...) */
 	    int x;			/* x coordinate of this field */
 	    char dirty;			/* this label has changed */
 	    char visible;		/* field is visible */
	} ent[8];
} SLK;

#define FIFO_SIZE	32

struct screen {
       	int		_ifd;	    	/* input file ptr for screen        */
   	FILE		*_ofp;	    	/* output file ptr for screen       */
   	int		_checkfd;	/* filedesc for typeahead check     */ 
#ifdef EXTERN_TERMINFO
	struct _terminal *_term;    	/* terminal type information        */
#else
	struct term	*_term;	    	/* terminal type information        */
#endif
	short		_lines;		/* screen lines			    */
	short		_columns;	/* screen columns		    */
	WINDOW		*_curscr;   	/* current screen                   */
	WINDOW		*_newscr;	/* virtual screen to be updated to  */
	WINDOW		*_stdscr;	/* screen's full-window context     */
	struct try  	*_keytry;   	/* "Try" for use with keypad mode   */
	unsigned int	_fifo[FIFO_SIZE]; 	/* input pushback buffer    */
	signed char	_fifohead, 	/* head of fifo queue               */
			_fifotail, 	/* tail of fifo queue               */
			_fifopeek;	/* where to peek for next char      */
	bool		_endwin;	/* are we out of window mode?       */
	chtype		_current_attr;	/* terminal attribute current set   */
	bool		_coloron;	/* is color enabled?                */
	int		_cursor;	/* visibility of the cursor         */
	int         	_cursrow;   	/* physical cursor row              */
	int         	_curscol;   	/* physical cursor column           */
	bool		_nl;	    	/* True if NL -> CR/NL is on        */
	bool		_raw;	    	/* True if in raw mode              */
	int		_cbreak;    	/* 1 if in cbreak mode              */
                       		    	/* > 1 if in halfdelay mode         */
	bool		_echo;	    	/* True if echo on                  */
	bool		_use_meta;      /* use the meta key?		    */
 	SLK		*_slk;	    	/* ptr to soft key struct / NULL    */
	int		_baudrate;	/* used to compute padding	    */

	/* cursor movement costs; units are 10ths of milliseconds */
	int		_char_padding;	/* cost of character put	    */
	int		_cr_cost;	/* cost of (carriage_return)	    */
	int		_cup_cost;	/* cost of (cursor_address)	    */
	int		_home_cost;	/* cost of (cursor_home)	    */
	int		_ll_cost;	/* cost of (cursor_to_ll)	    */
#ifdef TABS_OK
	int		_ht_cost;	/* cost of (tab)		    */
	int		_cbt_cost;	/* cost of (backtab)		    */
#endif /* TABS_OK */
	int		_cub1_cost;	/* cost of (cursor_left)	    */
	int		_cuf1_cost;	/* cost of (cursor_right)	    */
	int		_cud1_cost;	/* cost of (cursor_down)	    */
	int		_cuu1_cost;	/* cost of (cursor_up)		    */
	int		_cub_cost;	/* cost of (parm_cursor_left)	    */
	int		_cuf_cost;	/* cost of (parm_cursor_right)	    */
	int		_cud_cost;	/* cost of (parm_cursor_down)	    */
	int		_cuu_cost;	/* cost of (parm_cursor_up)	    */
	int		_hpa_cost;	/* cost of (column_address)	    */
	int		_vpa_cost;	/* cost of (row_address)	    */
};

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
#define _nc_alloc_screen() ((SP = (SCREEN *) calloc(1, sizeof(*SP))) != NULL)
#define _nc_set_screen(sp) SP = sp
#endif

/*
 * We don't want to use the lines or columns capabilities internally,
 * because if the application is running multiple screens under
 * X windows, it's quite possible they could all have type xterm
 * but have different sizes!  So...
 */
#define screen_lines	SP->_lines
#define screen_columns	SP->_columns

extern int _slk_init;			/* TRUE if slk_init() called */
extern int slk_initialize(WINDOW *, int);

#define MAXCOLUMNS    135
#define MAXLINES      66
#define UNINITIALISED ((struct try * ) -1)
