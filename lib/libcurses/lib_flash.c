/*	$OpenBSD: lib_flash.c,v 1.1 1997/12/03 05:21:17 millert Exp $	*/


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
 *	flash.c
 *
 *	The routine flash().
 *
 */

#include <curses.priv.h>
#include <term.h>	/* beep, flash */

MODULE_ID("Id: lib_flash.c,v 1.1 1997/10/08 05:59:49 jtc Exp $")

/*
 *	flash()
 *
 *	Flash the current terminal's screen if possible.   If not,
 *	sound the audible bell if one exists.
 *
 */

int flash(void)
{
	T((T_CALLED("flash()")));

	/* FIXME: should make sure that we are not in altchar mode */
	if (flash_screen) {
		TPUTS_TRACE("flash_screen");
		returnCode(putp(flash_screen));
	} else if (bell) {
		TPUTS_TRACE("bell");
		returnCode(putp(bell));
	}
	else
		returnCode(ERR);
}
