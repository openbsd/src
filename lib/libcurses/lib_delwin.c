
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
**	lib_delwin.c
**
**	The routine delwin().
**
*/

#include <curses.priv.h>

MODULE_ID("Id: lib_delwin.c,v 1.8 1997/02/01 23:22:54 tom Exp $")

static bool have_children(WINDOW *win)
{
	WINDOWLIST *p;
	for (p = _nc_windows; p != 0; p = p->next) {
		if (p->win->_flags & _SUBWIN
		 && p->win->_parent == win)
			return TRUE;
	}
	return FALSE;
}

int delwin(WINDOW *win)
{
	T((T_CALLED("delwin(%p)"), win));

	if (win == 0
	 || have_children(win))
		returnCode(ERR);

	if (win->_flags & _SUBWIN)
		touchwin(win->_parent);
	else if (curscr != 0)
		touchwin(curscr);

	_nc_freewin(win);

	returnCode(OK);
}
