
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
**	lib_scrreg.c
**
**	The routine wsetscrreg().
**
*/

#include <curses.priv.h>

MODULE_ID("Id: lib_scrreg.c,v 1.5 1997/02/01 23:22:54 tom Exp $")

int wsetscrreg(WINDOW *win, int top, int bottom)
{
	T((T_CALLED("wsetscrreg(%p,%d,%d)"), win, top, bottom));

	if (top >= 0  && top <= win->_maxy &&
		bottom >= 0  &&  bottom <= win->_maxy &&
		bottom > top)
	{
		win->_regtop	= (short)top;
		win->_regbottom = (short)bottom;

		returnCode(OK);
	} else
		returnCode(ERR);
}
