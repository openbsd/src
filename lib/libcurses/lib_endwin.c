
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

#include <curses.priv.h>
#include <term.h>

MODULE_ID("Id: lib_endwin.c,v 1.10 1997/02/02 00:36:41 tom Exp $")

int
endwin(void)
{
	T((T_CALLED("endwin()")));

	SP->_endwin = TRUE;

	_nc_mouse_wrap(SP);

	/* SP->_curs{row,col} may be used later in _nc_mvcur_wrap,save_curs */
	mvcur(-1, -1, SP->_cursrow = screen_lines - 1, SP->_curscol = 0);

	curs_set(1);	/* set cursor to normal mode */

	if (SP->_coloron == TRUE && orig_pair)
		putp(orig_pair);

	_nc_mvcur_wrap();	/* wrap up cursor addressing */

	if (SP  &&  (SP->_current_attr != A_NORMAL))
	    vidattr(A_NORMAL);

	/*
	 * Reset terminal's tab counter.  There's a long-time bug that
	 * if you exit a "curses" program such as vi or more, tab
	 * forward, and then backspace, the cursor doesn't go to the
	 * right place.  The problem is that the kernel counts the
	 * escape sequences that reset things as column positions.
	 * Utter a \r to reset this invisibly.
	 */
	_nc_outch('\r');

	returnCode(reset_shell_mode());
}
