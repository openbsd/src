
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
**	lib_mvwin.c
**
**	The routine mvwin().
**
*/

#include "curses.priv.h"

int mvwin(WINDOW *win, int by, int bx)
{
	T(("mvwin(%p,%d,%d) called", win, by, bx));

	if (win->_flags & _SUBWIN)
	    return(ERR);

	if (by + win->_maxy > screen_lines - 1
	||  bx + win->_maxx > screen_columns - 1
	||  by < 0
	||  bx < 0)
	    return(ERR);

	/*
	 * Whether or not the window is moved, touch the window's contents so
	 * that a following call to 'wrefresh()' will paint the window at the
	 * new location.  This ensures that if the caller has refreshed another
	 * window at the same location, that this one will be displayed.
	 */
	win->_begy = by;
	win->_begx = bx;
	return touchwin(win);
}
