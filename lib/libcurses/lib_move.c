
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
**	lib_move.c
**
**	The routine wmove().
**
*/

#include "curses.priv.h"

int
wmove(WINDOW *win, int y, int x)
{
	T(("wmove(%p,%d,%d) called", win, y, x));

	if (x >= 0  &&  x <= win->_maxx  &&
		y >= 0  &&  y <= win->_maxy)
	{
		win->_curx = (short)x;
		win->_cury = (short)y;

		win->_flags &= ~_NEED_WRAP;
		win->_flags |= _HASMOVED;
		return(OK);
	} else
		return(ERR);
}
