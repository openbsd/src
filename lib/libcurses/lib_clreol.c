/*	$OpenBSD: lib_clreol.c,v 1.3 1997/12/03 05:21:14 millert Exp $	*/


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

#include <curses.priv.h>

MODULE_ID("Id: lib_clreol.c,v 1.13 1997/09/20 15:02:34 juergen Exp $")

int  wclrtoeol(WINDOW *win)
{
int     code = ERR;
chtype	blank;
chtype	*ptr, *end;
short	y, x;

	T((T_CALLED("wclrtoeol(%p)"), win));

	if (win) {

	  y = win->_cury;
	  x = win->_curx;

	  /*
	   * If we have just wrapped the cursor, the clear applies to the new
	   * line, unless we are at the lower right corner.
	   */
	  if (win->_flags & _WRAPPED
	      && y < win->_maxy) {
	    win->_flags &= ~_WRAPPED;
	  }
	  
	  /*
	   * There's no point in clearing if we're not on a legal position,
	   * either.
	   */
	  if (win->_flags & _WRAPPED
	      || y > win->_maxy
	      || x > win->_maxx)
	    returnCode(ERR);
	  
	  blank = _nc_background(win);
	  end = &win->_line[y].text[win->_maxx];
	  
	  for (ptr = &win->_line[y].text[x]; ptr <= end; ptr++)
	    *ptr = blank;
	  
	  if (win->_line[y].firstchar > win->_curx
	      || win->_line[y].firstchar == _NOCHANGE)
	    win->_line[y].firstchar = win->_curx;
	  
	  win->_line[y].lastchar = win->_maxx;
	  
	  _nc_synchook(win);
	  code = OK;
	}
	returnCode(code);
}
