
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

MODULE_ID("Id: lib_data.c,v 1.8 1997/01/18 23:02:54 tom Exp $")

WINDOW *stdscr, *curscr, *newscr;

/*
 * Linked-list of all windows, to support '_nc_resizeall()' and '_nc_freeall()'
 */
WINDOWLIST *_nc_windows;

/*
 * These data correspond to the state of the idcok() and idlok() functions.  A
 * caveat is in order here:  the XSI and SVr4 documentation specify that these
 * functions apply to the window which is given as an argument.  However,
 * ncurses implements this logic only for the newscr/curscr update process,
 * _not_ per-window.
 */
bool _nc_idcok = TRUE;
bool _nc_idlok = FALSE;

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
