/*	$OpenBSD: lib_insch.c,v 1.3 1997/12/03 05:21:20 millert Exp $	*/


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
**	lib_insch.c
**
**	The routine winsch().
**
*/

#include <curses.priv.h>

MODULE_ID("Id: lib_insch.c,v 1.8 1997/09/20 15:02:34 juergen Exp $")

int  winsch(WINDOW *win, chtype c)
{
int code = ERR;
chtype	*temp1, *temp2;
chtype	*end;

	T((T_CALLED("winsch(%p, %s)"), win, _tracechtype(c)));

	if (win) {
	  end = &win->_line[win->_cury].text[win->_curx];
	  temp1 = &win->_line[win->_cury].text[win->_maxx];
	  temp2 = temp1 - 1;

	  while (temp1 > end)
	    *temp1-- = *temp2--;
	  
	  *temp1 = _nc_render(win, c);
	  
	  win->_line[win->_cury].lastchar = win->_maxx;
	  if (win->_line[win->_cury].firstchar == _NOCHANGE
	      ||  win->_line[win->_cury].firstchar > win->_curx)
	    win->_line[win->_cury].firstchar = win->_curx;
	  code = OK;
	}
	returnCode(code);
}
