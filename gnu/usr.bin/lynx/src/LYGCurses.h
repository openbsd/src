#ifndef __CURSES_LOADED
#define __CURSES_LOADED	1

#include <ssdef.h>
#include <stdio.h>
#include <smgdef.h>

#define	reg	register

#ifndef	TRUE
#define	TRUE	(1)
#define	FALSE	(0)
#endif
#define	ERR	(0)
#define	OK	(1)

#define	_SUBWIN		0001
#define	_ENDLINE	0002
#define	_FULLWIN	0004
#define	_SCROLLWIN	0010
#define	_FLUSH		0020
#define	_STANDOUT	0200

#define	_NOECHO		001
#define	_NONL		002
#define	_NOCRMODE	004
#define	_NORAW		010

#define	_BLINK		SMG$M_BLINK
#define	_BOLD		SMG$M_BOLD
#define	_REVERSE	SMG$M_REVERSE
#define	_UNDERLINE	SMG$M_UNDERLINE

struct	_win_st
{
	int	_cur_y, _cur_x;
	int	_max_y, _max_x;
	int	_beg_y, _beg_x;
	short	_flags;
	char	_clear, _leave, _scroll, _wrap;
	char	**_y;
	short	*_firstch, *_lastch;
	struct _win_st	*_next, *_parent, *_child;
	int	_id;
};


struct	_kb_st
{
	int	_id;
	unsigned char	_flags;
	struct
	{
		unsigned short	length;
		unsigned char	type;
		unsigned char	class;
		char		*address;
	}	_buffer_desc;
	int	_count;
	char	*_ptr;
};

struct	_pb_st
{
	int	_id;
	int	_rows, _cols;
	union	SMGDEF	*_attr;
	int	_attr_size;
};

#define	_KEYBOARD	struct _kb_st
#define	WINDOW		struct _win_st
#define	_PASTEBOARD	struct _pb_st


extern int LINES __asm("_$$PsectAttributes_NOSHR$$LINES");
extern int COLS __asm("_$$PsectAttributes_NOSHR$$COLS");
extern WINDOW *stdscr __asm("_$$PsectAttributes_NOSHR$$stdscr");
extern WINDOW *curscr __asm("_$$PsectAttributes_NOSHR$$curscr");
extern _KEYBOARD *stdkb __asm("_$$PsectAttributes_NOSHR$$stdkb");
extern _PASTEBOARD *stdpb __asm("_$$PsectAttributes_NOSHR$$stdpb");

#define	getch()		wgetch	(stdscr)
#define	addch(ch)	waddch	(stdscr, ch)
#define	addstr(string)	waddstr	(stdscr, string)
#define	move(y, x)	wmove	(stdscr, y, x)
#define	refresh()	wrefresh (stdscr)
#define	clear()		wclear	(stdscr)
#define	clrtobot()	wclrtobot (stdscr)
#define	clrtoeol()	wclrtoeol (stdscr)
#define	delch()		wdelch 	(stdscr)
#define	erase()		werase (stdscr)
#define	insch(ch)	winsch	(stdscr, ch)
#define	insertln()	winsertln (stdscr)
#define	standout()	wstandout (stdscr)
#define	standend()	wstandend (stdscr)
#define	getstr(string)	wgetstr	(stdscr, string)
#define	inch()		winch	(stdscr)
#define	setattr(attr)	wsetattr (stdscr, attr)
#define	clrattr(attr)	wclrattr (stdscr, attr)
#define	deleteln()	wdeleteln (stdscr)
#define	insstr(string)	winsstr (stdscr, string)

#define mvwaddch(win,y,x,ch)	(wmove(win,y,x)==ERR)?ERR:waddch(win,ch)
#define mvwgetch(win,y,x)	(wmove(win,y,x)==ERR)?ERR:wgetch(win)
#define mvwaddstr(win,y,x,str)	(wmove(win,y,x)==ERR)?ERR:waddstr(win,str)
#define mvwinsstr(win,y,x,str)	(wmove(win,y,x)==ERR)?ERR:winsstr(win,str)
#define mvwgetstr(win,y,x,str)	(wmove(win,y,x)==ERR)?ERR:wgetstr(win,str)
#define mvwinch(win,y,x)	(wmove(win,y,x)==ERR)?ERR:winch(win)
#define mvwdelch(win,y,x)	(wmove(win,y,x)==ERR)?ERR:wdelch(win)
#define mvwinsch(win,y,x,ch)	(wmove(win,y,x)==ERR)?ERR:winsch(win,ch)
#define mvwdeleteln(win,y,x)	(wmove(win,y,x)==ERR)?ERR:wdeleteln(win)
#define mvaddch(y,x,ch)	mvwaddch (stdscr, y, x, ch)
#define mvgetch(y,x)		mvwgetch (stdscr, y, x)
#define mvaddstr(y,x,str)	mvwaddstr (stdscr, y, x, str)
#define mvinsstr(y,x,str)	mvwinsstr (stdscr, y, x, str)
#define mvgetstr(y,x,str)	mvwgetstr (stdscr, y, x, str)
#define mvinch(y,x)		mvwinch (stdscr, y, x)
#define mvdelch(y,x)		mvwdelch (stdscr, y, x)
#define mvinsch(y,x,ch)	mvwinsch (stdscr, y, x, ch)
#define mvdeleteln(y,x)	mvwdeleteln (stdscr, y, x)
#define mvcur(ly,lx,ny,nx)	wmove (stdscr, ny, nx)
#pragma standard

#define clearok(win, bf)	(win->_clear = bf)
#define leaveok(win, bf)	(win->_leave = bf)
#define scrollok(win, bf)	(win->_scroll = bf)
#define wrapok(win, bf)	(win->_wrap = bf)
#define flushok(win,bf) (bf ? win->_flags |= _FLUSH : (win->_flags &= ~_FLUSH))
#define getyx(win,y,x)		y = win->_cur_y, x = win->_cur_x

#define echo()			(stdkb->_flags &= ~_NOECHO)
#define noecho()		(stdkb->_flags |= _NOECHO)
#define nl()			(stdkb->_flags &= ~_NONL)
#define nonl()			(stdkb->_flags |= _NONL)
#define crmode()		((stdkb->_flags &= ~_NOCRMODE), nonl ())
#define nocrmode()		(stdkb->_flags |= _NOCRMODE)
#define raw()			(stdkb->_flags &= ~_NORAW)
#define noraw()		(stdkb->_flags |= _NORAW)

#define check(status)	if (!(status & SS$_NORMAL))	\
			{	c$$translate (status); 	\
				return ERR;		\
			}

#define bool int

int waddch (WINDOW *win, char ch);

int waddstr (WINDOW *win, char *str);

int box (WINDOW *win, char vert, char hor);

int wclear (WINDOW *win);

int wclrattr (WINDOW *win, int attr);

int wclrtobot (WINDOW *win);

int wclrtoeol (WINDOW *win);

int wdelch (WINDOW *win);

int wdeleteln (WINDOW *win);

int delwin (WINDOW *win);

int endwin (void);

int werase (WINDOW *win);

int wgetch (WINDOW *win);

int wgetstr (WINDOW *win, char *str);

char winch (WINDOW *win);

WINDOW *initscr (void);

int winsch (WINDOW *win, char ch);

int winsertln (WINDOW *win);

int winsstr (WINDOW *win, char *str);

int longname (char *termbuf, char *name);

int mvwin (WINDOW *win, int st_row, int st_col);

int wmove (WINDOW *win, int y, int x);

WINDOW *newwin (int numlines, int numcols, int begin_y, int begin_x);

int overlay (WINDOW *win1, WINDOW *win2);

int overwrite (WINDOW *win1, WINDOW *win2);

#pragma NOSTANDARD
#undef printw
#undef wprintw
#undef wscanw
#undef scanw
#pragma STANDARD

int printw (char *format_spec, ...);

int wprintw (WINDOW *win, char *format_spec, ...);

int wrefresh (WINDOW *win);

int wscanw (WINDOW *win, char *format_spec, ...);

int	scanw (char *fmt, int arg1);

int scroll (WINDOW *win);

int wsetattr (WINDOW *win, int attr);

WINDOW *subwin (WINDOW *win, int numlines, int numcols,
			int begin_y, int begin_x);

int wstandend (WINDOW *win);

int wstandout (WINDOW *win);

int touchwin (WINDOW *win);

#if defined(CC$mixed_float) || defined(CC$VAXCSHR)

#ifndef CC$gfloat
#define CC$gfloat 0
#endif

#if CC$gfloat

#define printw  vaxc$gprintw
#define scanw   vaxc$gscanw
#define wprintw vaxc$gwprintw
#define wscanw  vaxc$gwscanw

#else

#define printw  vaxc$dprintw
#define scanw   vaxc$dscanw
#define wprintw vaxc$dwprintw
#define wscanw  vaxc$dwscanw

#endif
#endif

#endif					/* __CURSES_LOADED */
