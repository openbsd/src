
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

#include "curses.priv.h"

int wbkgd(WINDOW *win, const chtype ch)
{
int x, y;

	T(("wbkgd(%p, %lx) called", win, ch));
	wbkgdset(win, ch);

	for (y = 0; y <= win->_maxy; y++)
		for (x = 0; x <= win->_maxx; x++) 
			if ((win->_line[y].text[x]&A_CHARTEXT) == ' ')
				win->_line[y].text[x] |= ch;
			else
				win->_line[y].text[x] |= (ch&A_ATTRIBUTES);
	touchwin(win);
	_nc_synchook(win);
	return OK;
}

