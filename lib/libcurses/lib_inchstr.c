
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
**	lib_inchstr.c
**
**	The routine winchnstr().
**
*/

#include "curses.priv.h"

int winchnstr(WINDOW *win, chtype *str, int n)
{
	int	i;

	T(("winchnstr(%p,'%p',%d) called", win, str, n));

	for (i = 0; (n < 0 || (i < n)) && (win->_curx + i <= win->_maxx); i++)
	    str[i] = win->_line[win->_cury].text[win->_curx + i];
	str[i] = (chtype)0;

	return(i);
}
