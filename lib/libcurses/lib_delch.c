/*	$OpenBSD: lib_delch.c,v 1.3 1997/12/03 05:21:15 millert Exp $	*/


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
**	lib_delch.c
**
**	The routine wdelch().
**
*/

#include <curses.priv.h>

MODULE_ID("Id: lib_delch.c,v 1.6 1997/09/20 15:02:34 juergen Exp $")

int wdelch(WINDOW *win)
{
int     code = ERR;
chtype	*temp1, *temp2;
chtype	*end;
chtype	blank = _nc_background(win);

	T((T_CALLED("wdelch(%p)"), win));

	if (win) {
	  end = &win->_line[win->_cury].text[win->_maxx];
	  temp2 = &win->_line[win->_cury].text[win->_curx + 1];
	  temp1 = temp2 - 1;
	  
	  while (temp1 < end)
	    *temp1++ = *temp2++;
	  
	  *temp1 = blank;
	  
	  win->_line[win->_cury].lastchar = win->_maxx;
	  
	  if (win->_line[win->_cury].firstchar == _NOCHANGE
	      || win->_line[win->_cury].firstchar > win->_curx)
	    win->_line[win->_cury].firstchar = win->_curx;
	  
	  _nc_synchook(win);
	  code = OK;
	}
	returnCode(code);
}
