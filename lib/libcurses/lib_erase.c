
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
**	lib_erase.c
**
**	The routine werase().
**
*/

#include "curses.priv.h"

int  werase(WINDOW	*win)
{
int	y;
chtype	*sp, *end, *start, *maxx = NULL;
short	minx;

	T(("werase(%p) called", win));

	for (y = 0; y <= win->_maxy; y++) {
	    	minx = _NOCHANGE;
	    	start = win->_line[y].text;
	    	end = &start[win->_maxx];
	
	    	maxx = start;
	    	for (sp = start; sp <= end; sp++) {
		    	maxx = sp;
		    	if (minx == _NOCHANGE)
					minx = sp - start;
			*sp = _nc_render(win, *sp, BLANK);
	    	}

	    	if (minx != _NOCHANGE) {
			if (win->_line[y].firstchar > minx ||
		    	    win->_line[y].firstchar == _NOCHANGE)
		    		win->_line[y].firstchar = minx;

			if (win->_line[y].lastchar < maxx - win->_line[y].text)
		    	    win->_line[y].lastchar = maxx - win->_line[y].text;
	    	}
	}
	win->_curx = win->_cury = 0;
	win->_flags &= ~_NEED_WRAP;
	_nc_synchook(win);
	return OK;
}
