/*	$OpenBSD: lib_slkclear.c,v 1.1 1997/12/03 05:21:33 millert Exp $	*/


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
 *	lib_slkclear.c
 *	Soft key routines.
 *      Remove soft labels from the screen.
 */
#include <curses.priv.h>

MODULE_ID("Id: lib_slkclear.c,v 1.2 1997/10/18 18:08:12 tom Exp $")

int
slk_clear(void)
{
	T((T_CALLED("slk_clear()")));

	if (SP == NULL || SP->_slk == NULL)
		returnCode(ERR);
	SP->_slk->hidden = TRUE;
	/* For simulated SLK's it's looks much more natural to
	   inherit those attributes from the standard screen */
	SP->_slk->win->_bkgd  = stdscr->_bkgd;
	SP->_slk->win->_attrs = stdscr->_attrs;
	werase(SP->_slk->win);

	returnCode(wrefresh(SP->_slk->win));
}
