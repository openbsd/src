#ifndef LYCURSES_H
#define LYCURSES_H

#include "userdefs.h"

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
#if defined(UNIX) && !defined(unix)
#define unix
#endif /* UNIX && !unix */
#ifdef va_start
#undef va_start	 /* not used, undef to avoid warnings on some systems */
#endif /* va_start */
#include <slang.h>

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

#ifdef HAVE_CONFIG_H
# ifdef HAVE_NCURSES_H
#  include <ncurses.h>
# else
#  ifdef HAVE_CURSESX_H
#   include <cursesX.h>		/* Ultrix */
#  else
#   ifdef HAVE_JCURSES_H
#    include <jcurses.h>	/* sony_news */
#   else
#    include <curses.h>		/* default */
#   endif
#  endif
# endif

# if defined(wgetbkgd) && !defined(getbkgd)
#  define getbkgd(w) wgetbkgd(w)	/* workaround pre-1.9.9g bug */
# endif

# ifdef NCURSES
extern void LYsubwindow PARAMS((WINDOW * param));
# endif /* NCURSES */

#else
# if defined(VMS) && defined(__GNUC__)
#  include "LYGCurses.h"
# else
#  include <curses.h>  /* everything else */
# endif /* VMS && __GNUC__ */
#endif /* HAVE_CONFIG_H */

#ifdef VMS
extern void VMSbox PARAMS((WINDOW *win, int height, int width));
#else
extern void LYbox PARAMS((WINDOW *win, BOOLEAN formfield));
#endif /* VMS */
#endif /* USE_SLANG */


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

extern int LYlines;  /* replaces LINES */
extern int LYcols;   /* replaces COLS */

#ifndef HTUTILS_H
#include "HTUtils.h"
#endif /* HTUTILS_H */

extern void start_curses NOPARAMS;
extern void stop_curses NOPARAMS;
extern BOOLEAN setup PARAMS((char *terminal));
extern void LYstartTargetEmphasis NOPARAMS;
extern void LYstopTargetEmphasis NOPARAMS;

#ifdef VMS
extern void VMSexit();
extern int ttopen();
extern int ttclose();
extern int ttgetc();
extern void *VMSsignal PARAMS((int sig, void (*func)()));
#endif /* VMS */

#if defined(USE_COLOR_STYLE)
extern void curses_css PARAMS((char * name, int dir));
extern void curses_style PARAMS((int style, int dir, int previous));
extern void curses_w_style PARAMS((WINDOW* win, int style, int dir, int previous));
extern void setHashStyle PARAMS((int style, int color, int cattr, int mono, char* element));
extern void setStyle PARAMS((int style, int color, int cattr, int mono));
extern void wcurses_css PARAMS((WINDOW * win, char* name, int dir));
#define LynxChangeStyle curses_style
#else
extern int slang_style PARAMS((int style, int dir, int previous));
#define LynxChangeStyle slang_style
#endif /* USE_COLOR_STYLE */

#if USE_COLOR_TABLE
extern void LYaddAttr PARAMS((int a));
extern void LYsubAttr PARAMS((int a));
extern void lynx_setup_colors NOPARAMS;
extern unsigned int Lynx_Color_Flags;
#endif

#ifdef USE_SLANG
#if !defined(VMS) && !defined(DJGPP)
#define USE_SLANG_MOUSE		1
#endif /* USE_SLANG */

#define SL_LYNX_USE_COLOR	1
#define SL_LYNX_USE_BLINK	2
#define SL_LYNX_OVERRIDE_COLOR	4
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

#define addch SLsmg_write_char
#define echo()
#define printw SLsmg_printf

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
#define endwin() clear(),refresh(),SLsmg_reset_smg(),VTHome()
#else
#define endwin SLsmg_reset_smg(),SLang_reset_tty
#endif /* VMS */

#else /* Define curses functions: */

#ifdef FANCY_CURSES

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
#define wstart_reverse(a)	wsetattr(a, _REVERSE)
#define wstop_underline(a)	wclrattr(a, _UNDERLINE)
#define stop_reverse()		clrattr(_REVERSE)
#define wstop_reverse(a)	wclrattr(a, _REVERSE)

#else /* Not VMS: */

/*
 *  For Unix FANCY_FANCY curses we interpose
 *  our own functions to add or subtract the
 *  A_foo attributes. - FM
 */
#if USE_COLOR_TABLE
extern void LYaddWAttr PARAMS((WINDOW *win, int a));
extern void LYaddAttr PARAMS((int a));
extern void LYsubWAttr PARAMS((WINDOW *win, int a));
extern void LYsubAttr PARAMS((int a));
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
#define LYaddAttr		attrset
#define LYaddWAttr		wattrset
#define LYsubAttr		attroff
#define LYsubWAttr		wattroff
#endif

#ifdef UNDERLINE_LINKS
#define start_bold()		LYaddAttr(A_UNDERLINE)
#define stop_bold()		LYsubAttr(A_UNDERLINE)
#define start_underline()	LYaddAttr(A_BOLD)
#define stop_underline()	LYsubAttr(A_BOLD)
#else /* not UNDERLINE_LINKS: */
#define start_bold()		LYaddAttr(A_BOLD)
#define stop_bold()		LYsubAttr(A_BOLD)
#define start_underline()	LYaddAttr(A_UNDERLINE)
#define stop_underline()	LYsubAttr(A_UNDERLINE)
#endif /* UNDERLINE_LINKS */
#if defined(SNAKE) && defined(HP_TERMINAL)
#define start_reverse()		LYaddWAttr(stdscr, A_DIM)
#define wstart_reverse(a)	LYaddWAttr(a, A_DIM)
#define stop_reverse()		LYsubWAttr(stdscr, A_DIM)
#define wstop_reverse(a)	LYsubWAttr(a, A_DIM)
#else
#define start_reverse()		LYaddAttr(A_REVERSE)
#define wstart_reverse(a)	LYaddWAttr(a, A_REVERSE)
#define stop_reverse()		LYsubAttr(A_REVERSE)
#define wstop_reverse(a)	LYsubWAttr(a, A_REVERSE)
#endif /* SNAKE && HP_TERMINAL */
#endif /* VMS */

#else /* Not FANCY_CURSES: */

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
#endif /* USE_SLANG */

#ifdef USE_SLANG
#define LYGetYX(y, x)   y = SLsmg_get_row(), x = SLsmg_get_column()
#else
#ifdef getyx
#define LYGetYX(y, x)   getyx(stdscr, y, x)
#else
#define LYGetYX(y, x)   y = stdscr->_cury, x = stdscr->_curx
#endif /* getyx */
#endif /* USE_SLANG */

extern void lynx_enable_mouse PARAMS((int));
extern void lynx_force_repaint NOPARAMS;
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

#endif /* LYCURSES_H */
