/*	$OpenBSD: lib_vline.c,v 1.1 1997/12/03 05:21:40 millert Exp $	*/


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
**	lib_vline.c
**
**	The routine wvline().
**
*/

#include <curses.priv.h>

MODULE_ID("Id: lib_vline.c,v 1.1 1997/10/08 05:59:50 jtc Exp $")

int wvline(WINDOW *win, chtype ch, int n)
{
int   code = ERR;
short row, col;
short end;

	T((T_CALLED("wvline(%p,%s,%d)"), win, _tracechtype(ch), n));

	if (win) {
		row = win->_cury;
		col = win->_curx;
		end = row + n - 1;
		if (end > win->_maxy)
			end = win->_maxy;

		if (ch == 0)
			ch = ACS_VLINE;
		ch = _nc_render(win, ch);

		while(end >= row) {
			win->_line[end].text[col] = ch;
			if (win->_line[end].firstchar == _NOCHANGE
			 || win->_line[end].firstchar > col)
				win->_line[end].firstchar = col;
			if (win->_line[end].lastchar == _NOCHANGE
			 || win->_line[end].lastchar < col)
				win->_line[end].lastchar = col;
			end--;
		}

		_nc_synchook(win);
		code = OK;
	}
	returnCode(code);
}
