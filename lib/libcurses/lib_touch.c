
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
**	lib_touch.c
**
**	   The routines	untouchwin(),
**			wtouchln(),
**			is_linetouched()
**			is_wintouched().
**
*/

#include "curses.priv.h"

int is_linetouched(WINDOW *win, int line)
{
	if (line > win->_maxy || line < 0)
		return ERR;
	if (win->_line[line].firstchar != _NOCHANGE) return TRUE;
	return FALSE;
}

int is_wintouched(WINDOW *win)
{
int i;

	for (i = 0; i <= win->_maxy; i++)
		if (win->_line[i].firstchar != _NOCHANGE)
			return TRUE;
	return FALSE;
}

int wtouchln(WINDOW *win, int y, int n, int changed)
{
int i;

	T(("wtouchln(%p,%d,%d,%d)", win, y, n, changed));

	for (i = y; i < y+n; i++) {
		win->_line[i].firstchar = changed ? 0 : _NOCHANGE;
		win->_line[i].lastchar = changed ? win->_maxx : _NOCHANGE;
	}
	return OK;
}

