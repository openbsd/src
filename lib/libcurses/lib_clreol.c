
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
**	lib_clreol.c
**
**	The routine wclrtoeol().
**
*/

#include "curses.priv.h"

int  wclrtoeol(WINDOW *win)
{
chtype	*maxx, *ptr, *end;
short	y, x, minx;

	T(("wclrtoeol(%p) called", win));

	y = win->_cury;
	x = win->_curx;
	if (win->_flags & _NEED_WRAP
	 || y > win->_maxy
	 || x > win->_maxx)
	 	return ERR;

	end = &win->_line[y].text[win->_maxx];
	minx = _NOCHANGE;
	maxx = &win->_line[y].text[x];

	for (ptr = maxx; ptr <= end; ptr++) {
	    int blank = _nc_render(win, win->_line[y].text[x], BLANK);

	    if (*ptr != blank) {
			maxx = ptr;
			if (minx == _NOCHANGE)
			    minx = ptr - win->_line[y].text;
			*ptr = blank;
	    }
	}

	if (minx != _NOCHANGE) {
	    if (win->_line[y].firstchar > minx || win->_line[y].firstchar == _NOCHANGE)
			win->_line[y].firstchar = minx;

	    if (win->_line[y].lastchar < maxx - win->_line[y].text)
			win->_line[y].lastchar = maxx - win->_line[y].text;
	}
	_nc_synchook(win);
	return(OK);
}
