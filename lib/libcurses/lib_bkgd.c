
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

#include <curses.priv.h>

MODULE_ID("Id: lib_bkgd.c,v 1.7 1997/04/12 17:44:37 tom Exp $")

int wbkgd(WINDOW *win, const chtype ch)
{
int x, y;
chtype old_bkgd = getbkgd(win);
chtype new_bkgd = ch;

	T((T_CALLED("wbkgd(%p,%s)"), win, _tracechtype(new_bkgd)));

	if (TextOf(new_bkgd) == 0)
		new_bkgd |= BLANK;
	wbkgdset(win, new_bkgd);
	wattrset(win, AttrOf(new_bkgd));

	for (y = 0; y <= win->_maxy; y++) {
		for (x = 0; x <= win->_maxx; x++) {
			if (win->_line[y].text[x] == old_bkgd)
				win->_line[y].text[x] = new_bkgd;
			else
				win->_line[y].text[x] =
					TextOf(win->_line[y].text[x])
					| AttrOf(new_bkgd);
		}
	}
	touchwin(win);
	_nc_synchook(win);
	returnCode(OK);
}
