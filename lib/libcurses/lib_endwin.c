
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
**	lib_endwin.c
**
**	The routine endwin().
**
*/

#include "curses.priv.h"
#include "term.h"

int
endwin(void)
{
	T(("endwin() called"));

	SP->_endwin = TRUE;

	_nc_mouse_wrap(SP);

	mvcur(-1, -1, screen_lines - 1, 0);

	curs_set(1);	/* set cursor to normal mode */

	if (SP->_coloron == TRUE && orig_pair)
		putp(orig_pair);

 	_nc_mvcur_wrap();	/* wrap up cursor addressing */

	if (curscr  &&  (curscr->_attrs != A_NORMAL)) 
	    vidattr(curscr->_attrs = A_NORMAL);

	fflush(SP->_ofp);

	return(reset_shell_mode());
}
