/*	$OpenBSD: lib_wattron.c,v 1.1 1997/12/03 05:21:41 millert Exp $	*/


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
**	lib_wattron.c
**
**	The routines wattr_on().
**
*/

#include <curses.priv.h>
#include <ctype.h>

MODULE_ID("Id: lib_wattron.c,v 1.1 1997/10/08 05:59:51 jtc Exp $")

int wattr_on(WINDOW *win, const attr_t at)
{
	T((T_CALLED("wattr_on(%p,%s)"), win, _traceattr(at)));
	if (win) {
		T(("... current %s", _traceattr(win->_attrs)));
		toggle_attr_on(win->_attrs,at);
		returnCode(OK);
	} else
		returnCode(ERR);
}
