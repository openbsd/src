/*	$OpenBSD: lib_data.c,v 1.3 1997/12/03 05:21:14 millert Exp $	*/


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
**	lib_data.c
**
**	Common data that may/may not be allocated, but is referenced globally
**
*/

#include <curses.priv.h>

MODULE_ID("Id: lib_data.c,v 1.10 1997/09/03 15:27:09 Alexander.V.Lukyanov Exp $")

WINDOW *stdscr, *curscr, *newscr;

SCREEN *_nc_screen_chain;

/*
 * The variable 'SP' will be defined as a function on systems that cannot link
 * data-only modules, since it is used in a lot of places within ncurses and we
 * cannot guarantee that any application will use any particular function.  We
 * put the WINDOW variables in this module, because it appears that any
 * application that uses them will also use 'SP'.
 *
 * This module intentionally does not reference other ncurses modules, to avoid
 * module coupling that increases the size of the executable.
 */
#if BROKEN_LINKER
static	SCREEN *my_screen;

SCREEN *_nc_screen(void)
{
	return my_screen;
}

int _nc_alloc_screen(void)
{
	return ((my_screen = typeCalloc(SCREEN, 1)) != 0);
}

void _nc_set_screen(SCREEN *sp)
{
	my_screen = sp;
}
#else
SCREEN *SP;
#endif
