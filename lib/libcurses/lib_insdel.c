/*	$OpenBSD: lib_insdel.c,v 1.3 1997/12/03 05:21:20 millert Exp $	*/


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
**	lib_insdel.c
**
**	The routine winsdelln(win, n).
**  positive n insert n lines above current line
**  negative n delete n lines starting from current line
**
*/

#include <curses.priv.h>

MODULE_ID("Id: lib_insdel.c,v 1.7 1997/09/20 15:02:34 juergen Exp $")

int
winsdelln(WINDOW *win, int n)
{
int code = ERR;

	T((T_CALLED("winsdel(%p,%d)"), win, n));

	if (win) {
	  if (n != 0) {
	    _nc_scroll_window(win, -n, win->_cury, win->_maxy, _nc_background(win));	  
	    _nc_synchook(win);
	  }
	  code = OK;
	}
	returnCode(code);
}
