
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
**	lib_getstr.c
**
**	The routine wgetstr().
**
*/

#include "curses.priv.h"
#include "unctrl.h"
#include <string.h>

int wgetnstr(WINDOW *win, char *str, int maxlen)
{
bool	oldnl, oldecho, oldraw, oldcbreak, oldkeypad;
char	erasec;
char	killc;
char	*oldstr;
int ch;
  
	T(("wgetnstr(%p,%p, %d) called", win, str, maxlen));

	oldnl = SP->_nl;
	oldecho = SP->_echo;
	oldraw = SP->_raw;
	oldcbreak = SP->_cbreak;
	oldkeypad = win->_use_keypad;
	nl();
	noecho();
	noraw();
	cbreak();
	keypad(win, TRUE);

	erasec = erasechar();
	killc = killchar();

	oldstr = str;

	if (is_wintouched(win) || (win->_flags & _HASMOVED))
		wrefresh(win);

	while ((ch = wgetch(win)) != ERR) {
	        /*
		 * Some terminals (the Wyse-50 is the most common) generate
		 * a \n from the down-arrow key.  With this logic, it's the
		 * user's choice whether to set kcud=\n for wgetch();
		 * terminating *getstr() with \n should work either way.
		 */
		if (ch == '\n' || ch == '\r' || ch == KEY_DOWN)
			break;
	   	if (ch == erasec || ch == KEY_LEFT || ch == KEY_BACKSPACE) {
			if (str > oldstr) {
		    		str--;
		    		if (oldecho == TRUE)
			    		_nc_backspace(win);
			}
	 	} else if (ch == killc) {
			while (str > oldstr) {
			    	str--;
		    		if (oldecho == TRUE)
		    			_nc_backspace(win);
			}
		} else if (ch >= KEY_MIN
			   || (maxlen >= 0 && str - oldstr >= maxlen)) {
		    beep();
		} else {
			if (oldecho == TRUE) {
			        char	*glyph = unctrl(ch);

				mvwaddstr(curscr, win->_begy + win->_cury,
				  	win->_begx + win->_curx, glyph);
				waddstr(win, glyph);
				_nc_outstr(glyph);
				SP->_curscol += strlen(glyph);
			} 
			*str++ = ch;
	   	}
	}

    	win->_curx = 0;
	win->_flags &= ~_NEED_WRAP;
    	if (win->_cury < win->_maxy)
       		win->_cury++;
	wrefresh(win);

	if (oldnl == FALSE)
	    nonl();

	if (oldecho == TRUE)
	    echo();

	if (oldraw == TRUE)
	    raw();

	if (oldcbreak == FALSE)
	    nocbreak();

	if (oldkeypad == FALSE)
		keypad(win, FALSE);

	if (ch == ERR) {
		*str = '\0';
		return ERR;
	}
	*str = '\0';

	T(("wgetnstr returns \"%s\"", _nc_visbuf(oldstr)));

	return(OK);
}
