#ifndef LYCURSES_H
#define LYCURSES_H

#ifndef HTUTILS_H
#include <HTUtils.h>
#endif

/*
 * Because we have to configure PDCURSES last, we may get bogus definitions
 * from the system curses library - cancel these now.
 */
#ifdef HAVE_XCURSES

#undef ASSUME_DEFAULT_COLORS
#undef COLOR_CURSES
#undef FANCY_CURSES
#undef HAVE_CBREAK
#undef HAVE_RESIZETERM
#undef HAVE_USE_DEFAULT_COLORS
#undef NCURSES
#undef USE_DEFAULT_COLORS

#define HAVE_CBREAK 1
#define COLOR_CURSES 1
#define FANCY_CURSES 1

#endif

/*
 * The simple color scheme maps the 8 combinations of bold/underline/reverse
 * to the standard 8 ANSI colors (with some variations based on context).
 */
#undef USE_COLOR_TABLE

#ifndef USE_COLOR_STYLE
#if defined(USE_SLANG) || defined(COLOR_CURSES)
#define USE_COLOR_TABLE 1
#endif
#endif

#ifdef TRUE
#undef TRUE  /* to prevent parse error :( */
#endif /* TRUE */
#ifdef FALSE
#undef FALSE  /* to prevent parse error :( */
#endif /* FALSE */

#ifdef USE_SLANG
#include <slang.h>

#undef WINDOW
typedef struct {
    int	top_y;
    int left_x;
    int height;
    int width;
} WINDOW;

/* slang doesn't really do windows... */
#define waddch(w,c)  LYaddch(c)
#define waddstr(w,s) addstr(s)
#define wmove(win, row, col) SLsmg_gotorc((win)->top_y + (row), (win)->left_x + (col));

#ifndef SLSMG_UARROW_CHAR
#define SLSMG_UARROW_CHAR '^'
#endif

#ifndef SLSMG_DARROW_CHAR
#define SLSMG_DARROW_CHAR 'v'
#endif

#ifndef SLSMG_LARROW_CHAR
#define SLSMG_LARROW_CHAR '<'
#endif

#ifndef SLSMG_RARROW_CHAR
#define SLSMG_RARROW_CHAR '>'
#endif

#ifndef SLSMG_CKBRD_CHAR
#define SLSMG_CKBRD_CHAR '#'
#endif

#ifndef SLSMG_BLOCK_CHAR
#define SLSMG_BLOCK_CHAR '#'
#endif

#ifndef ACS_UARROW  
#define ACS_UARROW  SLSMG_UARROW_CHAR
#endif

#ifndef ACS_DARROW
#define ACS_DARROW  SLSMG_DARROW_CHAR
#endif

#ifndef ACS_LARROW
#define ACS_LARROW  SLSMG_LARROW_CHAR
#endif

#ifndef ACS_RARROW
#define ACS_RARROW  SLSMG_RARROW_CHAR
#endif

#ifndef ACS_CKBOARD
#define ACS_CKBOARD SLSMG_CKBRD_CHAR
#endif

#ifndef ACS_BLOCK
#define ACS_BLOCK   SLSMG_BLOCK_CHAR
#endif

#else /* Using curses: */

#ifdef VMS
#define FANCY_CURSES
#endif /* VMS */

/*
 *	CR may be defined before the curses.h include occurs.
 *	There is a conflict between the termcap char *CR and the define.
 *	Assuming that the definition of CR will always be carriage return.
 *	06-09-94 Lynx 2-3-1 Garrett Arch Blythe
 */
#ifdef CR
#undef CR  /* to prevent parse error :( */
#define REDEFINE_CR
#endif /* CR */

#ifdef HZ
#undef HZ  /* to prevent parse error :( */
#endif /* HZ */

/* SunOS 4.x has a redefinition between ioctl.h and termios.h */
#if defined(sun) && !defined(__SVR4)
#undef NL0
#undef NL1
#undef CR0
#undef CR1
#undef CR2
#undef CR3
#undef TAB0
#undef TAB1
#undef TAB2
#undef XTABS
#undef BS0
#undef BS1
#undef FF0
#undef FF1
#undef ECHO
#undef NOFLSH
#undef TOSTOP
#undef FLUSHO
#undef PENDIN
#endif

#if defined(_MSC_VER)
#undef MOUSE_MOVED	/* conflict between PDCURSES and _WIN32 */
#endif /* _MSC_VER */

/*
 * Do this to build with glibc 2.1.3 (apparently it was not used to build a
 * system before release).
 */
#include <signal.h>

#undef CS			/* some BSD versions of curses use this */
#define CS curses_CS		/* ...but we don't */

#ifdef ERR
#undef ERR			/* all versions of curses define this */
#endif

#ifdef HAVE_CONFIG_H
# ifdef HAVE_NCURSES_NCURSES_H
#   include <ncurses/ncurses.h>
# else
#  ifdef HAVE_NCURSES_H
#   include <ncurses.h>
#  else
#   ifdef HAVE_CURSESX_H
#    include <cursesX.h>	/* Ultrix */
#   else
#    ifdef HAVE_JCURSES_H
#     include <jcurses.h>	/* sony_news */
#    else
#     ifdef HAVE_XCURSES
#      include <xcurses.h>	/* PDCurses' UNIX port */
#     else
#      include <curses.h>	/* default */
#     endif
#    endif
#   endif
#  endif
# endif

# if defined(wgetbkgd) && !defined(getbkgd)
#  define getbkgd(w) wgetbkgd(w)	/* workaround pre-1.9.9g bug */
# endif

# ifdef FANCY_CURSES
#  if defined(NCURSES) && defined(HAVE_NCURSES_TERM_H)
#    include <ncurses/term.h>
#  else
#   if defined(HAVE_TERM_H)
#    include <term.h>
#   endif
#  endif
# endif

# if defined(NCURSES_VERSION) && defined(HAVE_DEFINE_KEY)
#  define USE_KEYMAPS		1
# endif

#else
# if defined(VMS) && defined(__GNUC__)
#  include <LYGCurses.h>
#  else
#   include <curses.h>  /* everything else */
# endif /* VMS && __GNUC__ */
#endif /* HAVE_CONFIG_H */

/*
 * PDCurses' mouse code does nothing in the DJGPP configuration.
 */
#if defined(PDCURSES) && !defined(__DJGPP__) && !defined(HAVE_XCURSES)
#define USE_MOUSE 1
#endif

/*
 * Pick up the native ncurses name:
 */
#if defined(NCURSES_MOUSE_VERSION)
#define USE_MOUSE 1
#endif

/*
 * If we have pads, use them to implement left/right scrolling.
 */
#if defined(HAVE_NEWPAD) && defined(HAVE_PNOUTREFRESH)
#define USE_CURSES_PADS 1
#endif

/*
 * ncurses 1.9.9e won't work for pads, but 4.2 does (1.9.9g doesn't have a
 * convenient ifdef, though it would work).
 */
#if defined(NCURSES_VERSION) && !defined(NCURSES_VERSION_MAJOR)
#undef USE_CURSES_PADS
#endif

#endif /* USE_SLANG */

#ifdef USE_SLANG
#define LYstopPopup() /* nothing */
#else
extern void LYsubwindow PARAMS((WINDOW * param));
#define LYstopPopup() LYsubwindow(0)
#endif /* NCURSES */

extern void LYbox PARAMS((WINDOW *win, BOOLEAN formfield));
extern WINDOW *LYstartPopup PARAMS((int top_y, int left_x, int height, int width));

/*
 * Useful macros not in PDCurses or very old ncurses headers.
 */
#if !defined(HAVE_GETATTRS) && !defined(getattrs)
#define getattrs(win) ((win)->_attrs)
#endif
#if !defined(HAVE_GETBEGX) && !defined(getbegx)
#define getbegx(win) ((win)->_begx)
#endif
#if !defined(HAVE_GETBEGY) && !defined(getbegy)
#define getbegy(win) ((win)->_begy)
#endif
#if !defined(HAVE_GETBKGD) && !defined(getbkgd)
#define getbkgd(win) ((win)->_bkgd)
#endif

#if defined(PDCURSES)
#define HAVE_GETBKGD 1	/* can use fallback definition */
#endif

/* Both slang and curses: */
#ifndef TRUE
#define TRUE  1
#endif /* !TRUE */
#ifndef FALSE
#define FALSE 0
#endif /* !FALSE */

#ifdef REDEFINE_CR
#define CR FROMASCII('\015')
#endif /* REDEFINE_CR */

#ifdef ALT_CHAR_SET
#define BOXVERT 0   /* use alt char set for popup window vertical borders */
#define BOXHORI 0   /* use alt char set for popup window vertical borders */
#endif

#ifndef BOXVERT
#define BOXVERT '*'	/* character for popup window vertical borders */
#endif
#ifndef BOXHORI
#define BOXHORI '*'	/* character for popup window horizontal borders */
#endif

#ifndef KEY_DOWN
#undef HAVE_KEYPAD	/* avoid confusion with bogus 'keypad()' */
#endif

extern int LYlines;	/* replaces LINES */
extern int LYcols;	/* replaces COLS */

#ifdef USE_CURSES_PADS
extern WINDOW *LYwin;
extern int LYshiftWin;
extern int LYwideLines;
extern int LYtableCols;
extern BOOL LYuseCursesPads;
#else
#define LYwin stdscr
#define LYshiftWin	0
#define LYwideLines	0
#define LYtableCols	0
#endif

#if defined(USE_COLOR_TABLE) || defined(USE_SLANG)
extern int Current_Attr;
extern int Masked_Attr;
#endif

extern BOOLEAN setup PARAMS((char *terminal));
extern int LYscreenHeight NOPARAMS;
extern int LYscreenWidth NOPARAMS;
extern void LYclear NOPARAMS;
extern void LYclrtoeol NOPARAMS;
extern void LYerase NOPARAMS;
extern void LYmove PARAMS((int y, int x));
extern void LYnoVideo PARAMS((int mask));
extern void LYpaddstr PARAMS((WINDOW *w, int width, CONST char *s));
extern void LYrefresh NOPARAMS;
extern void LYstartTargetEmphasis NOPARAMS;
extern void LYstopTargetEmphasis NOPARAMS;
extern void LYtouchline PARAMS((int row));
extern void LYwaddnstr PARAMS((WINDOW *w, CONST char *s, size_t len));
extern void start_curses NOPARAMS;
extern void stop_curses NOPARAMS;

#define LYaddstr(s)      LYwaddnstr(LYwin, s, strlen(s))
#define LYaddnstr(s,len) LYwaddnstr(LYwin, s, len)
#define LYwaddstr(w,s)   LYwaddnstr(w, s, strlen(s))

#ifdef VMS
extern int DCLsystem (char *command);
extern void VMSexit();
extern int ttopen();
extern int ttclose();
extern int ttgetc();
extern void VMSsignal PARAMS((int sig, void (*func)()));
#endif /* VMS */

#if defined(USE_COLOR_STYLE)
extern void curses_css PARAMS((char * name, int dir));
extern void curses_style PARAMS((int style, int dir));
extern void setHashStyle PARAMS((int style, int color, int cattr, int mono, char* element));
extern void setStyle PARAMS((int style, int color, int cattr, int mono));
extern void wcurses_css PARAMS((WINDOW * win, char* name, int dir));
extern void curses_w_style PARAMS((WINDOW* win, int style, int	dir));
#  define LynxChangeStyle(style,dir) curses_style(style,dir)
#  define LynxWChangeStyle(win,style,dir) curses_w_style(win,style,dir)
#else
#  define LynxWChangeStyle(win,style,dir)	(void)1
#endif /* USE_COLOR_STYLE */

#if USE_COLOR_TABLE
extern void LYaddAttr PARAMS((int a));
extern void LYsubAttr PARAMS((int a));
extern void lynx_setup_colors NOPARAMS;
extern unsigned int Lynx_Color_Flags;
#endif

#ifdef USE_SLANG
#define SHOW_WHEREIS_TARGETS 1

#if !defined(VMS) && !defined(DJGPP)
#define USE_MOUSE              1
#endif

#if !defined(__DJGPP__) && !defined(__CYGWIN__)
#define USE_KEYMAPS		1
#endif

#define SL_LYNX_USE_COLOR	1
#define SL_LYNX_OVERRIDE_COLOR	2

#define start_bold()      	LYaddAttr(1)
#define start_reverse()   	LYaddAttr(2)
#define start_underline() 	LYaddAttr(4)
#define stop_bold()       	LYsubAttr(1)
#define stop_reverse()    	LYsubAttr(2)
#define stop_underline()  	LYsubAttr(4)

#ifdef FANCY_CURSES
#undef FANCY_CURSES
#endif /* FANCY_CURSES */

/*
 *  Map some curses functions to slang functions.
 */
#define stdscr NULL
#ifdef SLANG_MBCS_HACK
extern int PHYSICAL_SLtt_Screen_Cols;
#define COLS PHYSICAL_SLtt_Screen_Cols
#else
#define COLS SLtt_Screen_Cols
#endif /* SLANG_MBCS_HACK */
#define LINES SLtt_Screen_Rows
#define move SLsmg_gotorc
#define addstr SLsmg_write_string
extern void LY_SLerase NOPARAMS;
#define erase LY_SLerase
#define clear LY_SLerase
#define standout SLsmg_reverse_video
#define standend  SLsmg_normal_video
#define clrtoeol SLsmg_erase_eol

#ifdef SLSMG_NEWLINE_SCROLLS
#define scrollok(a,b) SLsmg_Newline_Behavior \
   = ((b) ? SLSMG_NEWLINE_SCROLLS : SLSMG_NEWLINE_MOVES)
#else
#define scrollok(a,b) SLsmg_Newline_Moves = ((b) ? 1 : -1)
#endif

#define LYaddch(ch)   SLsmg_write_char(ch)
#define addch_raw(ch) do {                                \
                        SLsmg_Char_Type buf;              \
                        buf = (ch) | (Current_Attr << 4); \
                        SLsmg_write_raw (&buf, 1);        \
                      } while (0)
#define echo()
#define printw        SLsmg_printf

extern int curscr;
extern BOOLEAN FullRefresh;
#ifdef clearok
#undef clearok
#endif /* clearok */
#define clearok(a,b) { FullRefresh = (BOOLEAN)b; }
extern void LY_SLrefresh NOPARAMS;
#ifdef refresh
#undef refresh
#endif /* refresh */
#define refresh LY_SLrefresh

#ifdef VMS
extern void VTHome NOPARAMS;
#define endwin() LYclear(),refresh(),SLsmg_reset_smg(),VTHome()
#else
#define endwin SLsmg_reset_smg(),SLang_reset_tty
#endif /* VMS */

#else /* Define curses functions: */

#ifdef FANCY_CURSES
#define SHOW_WHEREIS_TARGETS 1

#ifdef VMS
/*
 *  For VMS curses, [w]setattr() and [w]clrattr()
 *  add and subtract, respectively, the attributes
 *  _UNDERLINE, _BOLD, _REVERSE, and _BLINK. - FM
 */
#ifdef UNDERLINE_LINKS
#define start_bold()		setattr(_UNDERLINE)
#define stop_bold()		clrattr(_UNDERLINE)
#define start_underline()	setattr(_BOLD)
#define stop_underline()	clrattr(_BOLD)
#else /* not UNDERLINE_LINKS */
#define start_bold()		setattr(_BOLD)
#define stop_bold()		clrattr(_BOLD)
#define start_underline()	setattr(_UNDERLINE)
#define stop_underline()	clrattr(_UNDERLINE)
#endif /* UNDERLINE_LINKS */
#define start_reverse()		setattr(_REVERSE)
#define wstart_reverse(w)	wsetattr(w, _REVERSE)
#define stop_reverse()		clrattr(_REVERSE)
#define wstop_reverse(w)	wclrattr(w, _REVERSE)

#else /* Not VMS: */

extern int string_to_attr PARAMS((char *name));

/*
 *  For Unix FANCY_FANCY curses we interpose
 *  our own functions to add or subtract the
 *  A_foo attributes. - FM
 */
#if USE_COLOR_TABLE
extern void LYaddWAttr PARAMS((WINDOW *win, int a));
extern void LYsubWAttr PARAMS((WINDOW *win, int a));
extern void LYaddWAttr PARAMS((WINDOW *win, int a));
extern void LYsubWAttr PARAMS((WINDOW *win, int a));
extern void lynx_set_color PARAMS((int a));
extern void lynx_standout  PARAMS((int a));
extern int  lynx_chg_color PARAMS((int, int, int));
#undef  standout
#define standout() 		lynx_standout(TRUE)
#undef  standend
#define standend() 		lynx_standout(FALSE)
#else
#define LYaddAttr(attr)		LYaddWAttr(LYwin,attr)
#define LYaddWAttr(win,attr)	wattron(win,attr)
#define LYsubAttr(attr)		LYsubWAttr(LYwin,attr)
#define LYsubWAttr(win,attr)	wattroff(win,attr)
#endif

#ifdef UNDERLINE_LINKS
#define start_bold()		LYaddAttr(A_UNDERLINE)
#define stop_bold()		LYsubAttr(A_UNDERLINE)
#ifdef __CYGWIN__	/* 1999/02/25 (Thu) 01:09:45 */
#define start_underline()	/* LYaddAttr(A_BOLD) */
#define stop_underline()	/* LYsubAttr(A_BOLD) */
#else
#define start_underline()	LYaddAttr(A_BOLD)
#define stop_underline()	LYsubAttr(A_BOLD)
#endif /* __CYGWIN__ */
#else /* not UNDERLINE_LINKS: */
#define start_bold()		LYaddAttr(A_BOLD)
#define stop_bold()		LYsubAttr(A_BOLD)
#ifdef USE_COLOR_STYLE
#define start_underline()	attron(A_UNDERLINE) /* allow combining - kw */
#else
#define start_underline()	LYaddAttr(A_UNDERLINE)
#endif /* USE_COLOR_STYLE */
#define stop_underline()	LYsubAttr(A_UNDERLINE)
#endif /* UNDERLINE_LINKS */

#if defined(SNAKE) && defined(HP_TERMINAL)
#define start_reverse()		LYaddWAttr(LYwin, A_DIM)
#define wstart_reverse(w)	LYaddWAttr(w, A_DIM)
#define stop_reverse()		LYsubWAttr(LYwin, A_DIM)
#define wstop_reverse(w)	LYsubWAttr(w, A_DIM)
#else
#define start_reverse()		LYaddAttr(A_REVERSE)
#define wstart_reverse(w)	LYaddWAttr(w, A_REVERSE)
#define stop_reverse()		LYsubAttr(A_REVERSE)
#define wstop_reverse(w)	LYsubWAttr(w, A_REVERSE)
#endif /* SNAKE && HP_TERMINAL */

#endif /* VMS */

#else /* Not FANCY_CURSES: */

#ifdef COLOR_CURSES
#undef COLOR_CURSES
Error FANCY_CURSES
There is a problem with the configuration.  We expect to have FANCY_CURSES
defined when COLOR_CURSES is defined, since we build on the attributes used in
FANCY_CURSES.  Check your config.log to see why the FANCY_CURSES test failed.
#endif

/*
 *  We only have [w]standout() and [w]standin(),
 *  so we'll use them synonymously for bold and
 *  reverse, and ignore underline. - FM
 */
#define start_bold()		standout()
#define start_underline()	/* nothing */
#define start_reverse()		standout()
#define wstart_reverse(a)	wstandout(a)
#define stop_bold()		standend()
#define stop_underline()	/* nothing */
#define stop_reverse()		standend()
#define wstop_reverse(a)	wstandend(a)

#endif /* FANCY_CURSES */

#ifndef ACS_UARROW  
#define ACS_UARROW  '^'
#endif

#ifndef ACS_DARROW
#define ACS_DARROW  'V'
#endif

#ifndef ACS_LARROW
#define ACS_LARROW '{'
#endif

#ifndef ACS_RARROW
#define ACS_RARROW '}'
#endif

#define LYaddch(ch)		waddch(LYwin, ch)

#define addch_raw(ch)           LYaddch(ch)

#endif /* USE_SLANG */

#ifdef USE_SLANG
#define LYGetYX(y, x)   y = SLsmg_get_row(), x = SLsmg_get_column()
#else
#ifdef getyx
#define LYGetYX(y, x)   getyx(LYwin, y, x)
#else
#define LYGetYX(y, x)   y = LYwin->_cury, x = LYwin->_curx
#endif /* getyx */
#endif /* USE_SLANG */

/*
 * If the screen library allows us to specify "default" color, allow user to 
 * control it.
 */
#if USE_DEFAULT_COLORS
#if USE_SLANG || (HAVE_ASSUME_DEFAULT_COLORS && !defined(USE_COLOR_STYLE))
#define EXP_ASSUMED_COLOR 1
#endif
#endif

extern void lynx_enable_mouse PARAMS((int));
extern void lynx_force_repaint NOPARAMS;
extern void lynx_nl2crlf PARAMS((int normal));
extern void lynx_start_title_color NOPARAMS;
extern void lynx_stop_title_color NOPARAMS;
extern void lynx_start_link_color PARAMS((int flag, int pending));
extern void lynx_stop_link_color PARAMS((int flag, int pending));
extern void lynx_stop_target_color NOPARAMS;
extern void lynx_start_target_color NOPARAMS;
extern void lynx_start_status_color NOPARAMS;
extern void lynx_stop_status_color NOPARAMS;
extern void lynx_start_h1_color NOPARAMS;
extern void lynx_stop_h1_color NOPARAMS;
extern void lynx_start_prompt_color NOPARAMS;
extern void lynx_stop_prompt_color NOPARAMS;
extern void lynx_start_radio_color NOPARAMS;
extern void lynx_stop_radio_color NOPARAMS;
extern void lynx_stop_all_colors NOPARAMS;

/*
 * To prevent corrupting binary data on DOS, MS-WINDOWS or OS/2 we open files
 * and stdout in BINARY mode by default.  Where necessary we should open and
 * (close!) TEXT mode.
 *
 * Note:  EMX has no corresponding variable like _fmode on DOS, but it does
 * have setmode.
 */
#if defined(_WINDOWS) || defined(DJGPP) || defined(__EMX__) || defined(WIN_EX)
#define SetOutputMode(mode) fflush(stdout), setmode(fileno(stdout), mode)
#else
#define SetOutputMode(mode) /* nothing */
#endif

#if defined(_WINDOWS) || defined(DJGPP)
#define SetDefaultMode(mode) _fmode = mode
#else
#define SetDefaultMode(mode) /* nothing */
#endif

/*
 * Very old versions of curses cannot put the cursor on the lower right corner.
 * Adjust our "hidden" cursor position accordingly.
 */
#if defined(FANCY_CURSES) || defined(USE_SLANG)
#define LYHideCursor() LYmove((LYlines - 1), (LYcols - 1))
#else
#define LYHideCursor() LYmove((LYlines - 1), (LYcols - 2))
#endif

extern void LYstowCursor PARAMS((WINDOW * win, int row, int col));

#endif /* LYCURSES_H */
