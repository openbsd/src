/*	$OpenBSD: lib_insstr.c,v 1.3 1997/12/03 05:21:21 millert Exp $	*/


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
**	lib_insstr.c
**
**	The routine winsnstr().
**
*/

#include <curses.priv.h>
#include <ctype.h>

MODULE_ID("Id: lib_insstr.c,v 1.10 1997/09/20 15:02:34 juergen Exp $")

int winsnstr(WINDOW *win, const char *str, int n)
{
int     code = ERR;
short	oy;
short	ox ;
const char *cp;

        T((T_CALLED("winsstr(%p,%s,%d)"), win, _nc_visbuf(str), n));

	if (win && str) {
	  oy = win->_cury; ox = win->_curx;
	  for (cp = str; *cp && (n <= 0 || (cp - str) < n); cp++) {
	    if (*cp == '\n' || *cp == '\r' || *cp == '\t' || *cp == '\b')
	      _nc_waddch_nosync(win, (chtype)(*cp));
	    else if (is7bits(*cp) && iscntrl(*cp)) {
	      winsch(win, ' ' + (chtype)(*cp));
	      winsch(win, '^');
	      win->_curx += 2;
	    } else {
	      winsch(win, (chtype)(*cp));
	      win->_curx++;
	    }
	    if (win->_curx > win->_maxx)
	      win->_curx = win->_maxx;
	  }
	  
	  win->_curx = ox;
	  win->_cury = oy;
	  _nc_synchook(win);
	  code = OK;
	}
	returnCode(code);
}
