/*	$OpenBSD: lib_slkinit.c,v 1.1 1997/12/03 05:21:33 millert Exp $	*/


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
 *	lib_slkinit.c
 *	Soft key routines.
 *      Initialize soft labels.  Called by the user before initscr().
 */
#include <curses.priv.h>

MODULE_ID("Id: lib_slkinit.c,v 1.2 1997/10/18 18:05:46 tom Exp $")

int
slk_init(int format)
{
	T((T_CALLED("slk_init(%d)"), format));
	if (format < 0 || format > 3)
		returnCode(ERR);
	_nc_slk_format = 1 + format;
	returnCode(OK);
}
