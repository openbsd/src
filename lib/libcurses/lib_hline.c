/*	$OpenBSD: lib_hline.c,v 1.1 1997/12/03 05:21:19 millert Exp $	*/


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
**	lib_hline.c
**
**	The routine whline().
**
*/

#include <curses.priv.h>

MODULE_ID("Id: lib_hline.c,v 1.1 1997/10/08 05:59:50 jtc Exp $")

int whline(WINDOW *win, chtype ch, int n)
{
int   code = ERR;
short line;
short start;
short end;

	T((T_CALLED("whline(%p,%s,%d)"), win, _tracechtype(ch), n));

	if (win) {
		line  = win->_cury;
		start = win->_curx;
		end   = start + n - 1;
		if (end > win->_maxx)
			end   = win->_maxx;

		if (win->_line[line].firstchar == _NOCHANGE
		 || win->_line[line].firstchar > start)
			win->_line[line].firstchar = start;
		if (win->_line[line].lastchar == _NOCHANGE
		 || win->_line[line].lastchar < start)
			win->_line[line].lastchar = end;

		if (ch == 0)
			ch = ACS_HLINE;
		ch = _nc_render(win, ch);

		while ( end >= start) {
			win->_line[line].text[end] = ch;
			end--;
		}
		code = OK;
	}
	returnCode(code);
}
