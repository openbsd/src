/*	$OpenBSD: lib_scroll.c,v 1.3 1997/12/03 05:21:30 millert Exp $	*/


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
**	lib_scroll.c
**
**	The routine wscrl(win, n).
**  positive n scroll the window up (ie. move lines down)
**  negative n scroll the window down (ie. move lines up)
**
*/

#include <curses.priv.h>

MODULE_ID("Id: lib_scroll.c,v 1.15 1997/09/20 15:02:34 juergen Exp $")

void _nc_scroll_window(WINDOW *win, int const n, short const top, short const bottom, chtype blank)
{
int	line, j;
size_t	to_copy = (size_t)(sizeof(chtype) * (win->_maxx + 1));

	TR(TRACE_MOVE, ("_nc_scroll_window(%p, %d, %d, %d)", win, n, top,bottom)); 

	/*
	 * This used to do a line-text pointer-shuffle instead of text copies.
	 * That (a) doesn't work when the window is derived and doesn't have
	 * its own storage, (b) doesn't save you a lot on modern machines
	 * anyway.  Your typical memcpy implementations are coded in
	 * assembler using a tight BLT loop; for the size of copies we're
	 * talking here, the total execution time is dominated by the one-time
	 * setup cost.  So there is no point in trying to be excessively
	 * clever -- esr.
	 */

	/* shift n lines downwards */
    	if (n < 0) {
		for (line = bottom; line >= top-n; line--) {
		    	memcpy(win->_line[line].text,
			       win->_line[line+n].text,
			       to_copy);
			if_USE_SCROLL_HINTS(win->_line[line].oldindex = win->_line[line+n].oldindex);
		}
		for (line = top; line < top-n; line++) {
			for (j = 0; j <= win->_maxx; j ++)
				win->_line[line].text[j] = blank;
			if_USE_SCROLL_HINTS(win->_line[line].oldindex = _NEWINDEX);
		}
    	}

	/* shift n lines upwards */
    	if (n > 0) {
		for (line = top; line <= bottom-n; line++) {
		    	memcpy(win->_line[line].text,
			       win->_line[line+n].text,
			       to_copy);
			if_USE_SCROLL_HINTS(win->_line[line].oldindex = win->_line[line+n].oldindex);
		}
		for (line = bottom; line > bottom-n; line--) {
			for (j = 0; j <= win->_maxx; j ++)
				win->_line[line].text[j] = blank;
			if_USE_SCROLL_HINTS(win->_line[line].oldindex = _NEWINDEX);
		}
	}
	touchline(win, top, bottom-top+1);
}

int
wscrl(WINDOW *win, int n)
{
	T((T_CALLED("wscrl(%p,%d)"), win, n));

	if (!win || !win->_scroll)
		returnCode(ERR);

	if (n == 0)
		returnCode(OK);

	if ((n > (win->_regbottom - win->_regtop)) || 
	    (-n > (win->_regbottom - win->_regtop)))
	    returnCode(ERR);

	_nc_scroll_window(win, n, win->_regtop, win->_regbottom, _nc_background(win));

	_nc_synchook(win);
    	returnCode(OK);
}
