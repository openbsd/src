
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
**	lib_addstr.c
*
**	The routines waddnstr(), waddchnstr().
**
*/

#include <curses.priv.h>

MODULE_ID("Id: lib_addstr.c,v 1.11 1997/03/08 21:38:52 tom Exp $")

int
waddnstr(WINDOW *win, const char *const astr, int n)
{
unsigned const char *str = (unsigned const char *)astr;
int code = ERR;

	T((T_CALLED("waddnstr(%p,%s,%d)"), win, _nc_visbuf(astr), n));
	T(("... current %s", _traceattr(win->_attrs)));

	if (str != 0) {

		TR(TRACE_VIRTPUT, ("str is not null"));
		code = OK;
		if (n < 0)
			n = (int)strlen(astr);

		while((n-- > 0) && (*str != '\0')) {
			TR(TRACE_VIRTPUT, ("*str = %#x", *str));
			if (_nc_waddch_nosync(win, (chtype)*str++) == ERR) {
				code = ERR;
				break;
			}
		}
	}
	_nc_synchook(win);
	TR(TRACE_VIRTPUT, ("waddnstr returns %d", code));
	returnCode(code);
}

int
waddchnstr(WINDOW *win, const chtype *const astr, int n)
{
short oy = win->_cury;
short ox = win->_curx;
const chtype *str = (const chtype *)astr;
int code = OK;

	T((T_CALLED("waddchnstr(%p,%p,%d)"), win, str, n));

	if (n < 0) {
		n = 0;
		while (*str++ != 0)
			n++;
		str = (const chtype *)astr;
	}

	while(n-- > 0) {
		if (_nc_waddch_nosync(win, *str++) == ERR) {
			code = ERR;
			break;
		}
	}

	win->_curx = ox;
	win->_cury = oy;
	_nc_synchook(win);
	returnCode(code);
}
