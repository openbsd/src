
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
**	lib_clrbot.c
**
**	The routine wclrtobot().
**
*/

#include "curses.priv.h"

int wclrtobot(WINDOW *win)
{
chtype	*ptr, *end, *maxx = NULL;
short	y, startx, minx;

	T(("wclrtobot(%p) called", win));

	startx = win->_curx;
	if (win->_flags & _NEED_WRAP)
		startx++;

	T(("clearing from y = %d to y = %d with maxx =  %d", win->_cury, win->_maxy, win->_maxx));

	for (y = win->_cury; y <= win->_maxy; y++) {
		minx = _NOCHANGE;
		end = &win->_line[y].text[win->_maxx];

		for (ptr = &win->_line[y].text[startx]; ptr <= end; ptr++) {
			int blank = _nc_render(win, *ptr, BLANK);

			if (*ptr != blank) {
				maxx = ptr;
				if (minx == _NOCHANGE)
					minx = ptr - win->_line[y].text;
				*ptr = blank;
			}
		}

		if (minx != _NOCHANGE) {
			if (win->_line[y].firstchar > minx
					||  win->_line[y].firstchar == _NOCHANGE)
			    win->_line[y].firstchar = minx;

			if (win->_line[y].lastchar < maxx - win->_line[y].text)
			    win->_line[y].lastchar = maxx - win->_line[y].text;
		}

		startx = 0;
	}
	_nc_synchook(win);
	return OK;
}
