/*	$OpenBSD: lib_slklab.c,v 1.1 1997/12/03 05:21:34 millert Exp $	*/


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
 *	lib_slklab.c
 *	Soft key routines.
 *      Fetch the label text.
 */
#include <curses.priv.h>

MODULE_ID("Id: lib_slklab.c,v 1.3 1997/10/18 19:02:06 tom Exp $")

char*
slk_label(int n)
{
	T((T_CALLED("slk_label(%d)"), n));

	if (SP == NULL || SP->_slk == NULL || n < 1 || n > SP->_slk->labcnt)
		returnPtr(0);
	returnPtr(SP->_slk->ent[n-1].text);
}
