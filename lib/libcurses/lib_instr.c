/*	$OpenBSD: lib_instr.c,v 1.3 1997/12/03 05:21:21 millert Exp $	*/


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
**	lib_instr.c
**
**	The routine winnstr().
**
*/

#include <curses.priv.h>

MODULE_ID("Id: lib_instr.c,v 1.7 1997/09/20 15:02:34 juergen Exp $")

int winnstr(WINDOW *win, char *str, int n)
{
	int	i=0, row, col;

	T((T_CALLED("winnstr(%p,%p,%d)"), win, str, n));

	if (!str)
	  returnCode(0);
	
	if (win) {
	  getyx(win, row, col);

	  if (n < 0)
	    n = win->_maxx - win->_curx + 1;

	  for (; i < n;) {
	    str[i++] = TextOf(win->_line[row].text[col]);
	    if (++col > win->_maxx) {
	      col = 0;
	      if (++row > win->_maxy)
		break;
	    }
	  }
	}
	str[i] = '\0';	/* SVr4 does not seem to count the null */
	returnCode(i);
}

