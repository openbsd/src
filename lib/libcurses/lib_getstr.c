
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
**	lib_getstr.c
**
**	The routine wgetstr().
**
*/

#include <curses.priv.h>
#include <term.h>

MODULE_ID("Id: lib_getstr.c,v 1.11 1997/02/01 23:22:54 tom Exp $")

/*
 * This wipes out the last character, no matter whether it was a tab, control
 * or other character, and handles reverse wraparound.
 */
static char *WipeOut(WINDOW *win, int y, int x, char *first, char *last, bool echoed)
{
	if (last > first) {
		*--last = '\0';
		if (echoed) {
			int y1 = win->_cury;
			int x1 = win->_curx;

			wmove(win, y, x);
			waddstr(win, first);
			getyx(win, y, x);
			while (win->_cury < y1
			   || (win->_cury == y1 && win->_curx < x1))
				waddch(win, ' ');

			wmove(win, y, x);
		}
	}
	return last;
}

int wgetnstr(WINDOW *win, char *str, int maxlen)
{
TTY	buf;
bool	oldnl, oldecho, oldraw, oldcbreak, oldkeypad;
char	erasec;
char	killc;
char	*oldstr;
int ch;
int	y, x;

	T((T_CALLED("wgetnstr(%p,%p, %d)"), win, str, maxlen));

	GET_TTY(cur_term->Filedes, &buf);

	oldnl = SP->_nl;
	oldecho = SP->_echo;
	oldraw = SP->_raw;
	oldcbreak = SP->_cbreak;
	oldkeypad = win->_use_keypad;
	nl();
	noecho();
	noraw();
	cbreak();
	keypad(win, TRUE);

	erasec = erasechar();
	killc = killchar();

	oldstr = str;
	getyx(win, y, x);

	if (is_wintouched(win) || (win->_flags & _HASMOVED))
		wrefresh(win);

	while ((ch = wgetch(win)) != ERR) {
		/*
		 * Some terminals (the Wyse-50 is the most common) generate
		 * a \n from the down-arrow key.  With this logic, it's the
		 * user's choice whether to set kcud=\n for wgetch();
		 * terminating *getstr() with \n should work either way.
		 */
		if (ch == '\n'
		 || ch == '\r'
		 || ch == KEY_DOWN
		 || ch == KEY_ENTER)
			break;
		if (ch == erasec || ch == KEY_LEFT || ch == KEY_BACKSPACE) {
			if (str > oldstr) {
				str = WipeOut(win, y, x, oldstr, str, oldecho);
			}
		} else if (ch == killc) {
			while (str > oldstr) {
				str = WipeOut(win, y, x, oldstr, str, oldecho);
			}
		} else if (ch >= KEY_MIN
			   || (maxlen >= 0 && str - oldstr >= maxlen)) {
			beep();
		} else {
			*str++ = ch;
			if (oldecho == TRUE) {
				if (waddch(win, ch) == ERR) {
					/*
					 * We can't really use the lower-right
					 * corner for input, since it'll mess
					 * up bookkeeping for erases.
					 */
					win->_flags &= ~_WRAPPED;
					waddch(win, ' ');
					str = WipeOut(win, y, x, oldstr, str, oldecho);
					continue;
				}
				wrefresh(win);
			}
		}
	}

	win->_curx = 0;
	win->_flags &= ~_WRAPPED;
	if (win->_cury < win->_maxy)
		win->_cury++;
	wrefresh(win);

	/* Restore with a single I/O call, to fix minor asymmetry between
	 * raw/noraw, etc.
	 */
	SP->_nl = oldnl;
	SP->_echo = oldecho;
	SP->_raw = oldraw;
	SP->_cbreak = oldcbreak;

	SET_TTY(cur_term->Filedes, &buf);

	if (oldkeypad == FALSE)
		keypad(win, FALSE);

	*str = '\0';
	if (ch == ERR)
		returnCode(ERR);

	T(("wgetnstr returns %s", _nc_visbuf(oldstr)));

	returnCode(OK);
}
