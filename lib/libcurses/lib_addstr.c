/*	$OpenBSD: lib_addstr.c,v 1.3 1997/12/03 05:21:11 millert Exp $	*/


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
**	lib_addstr.c
*
**	The routines waddnstr(), waddchnstr().
**
*/

#include <curses.priv.h>

MODULE_ID("Id: lib_addstr.c,v 1.13 1997/09/20 15:02:34 juergen Exp $")

int
waddnstr(WINDOW *win, const char *const astr, int n)
{
unsigned const char *str = (unsigned const char *)astr;
int code = ERR;

	T((T_CALLED("waddnstr(%p,%s,%d)"), win, _nc_visbuf(astr), n));
	  
	if (win && (str != 0)) {	    
	  T(("... current %s", _traceattr(win->_attrs)));
	  TR(TRACE_VIRTPUT, ("str is not null"));
	  code = OK;
	  if (n < 0)
	    n = (int)strlen(astr);
	  
	  while((n-- > 0) && (*str != '\0')) {
	    TR(TRACE_VIRTPUT, ("*str = %#x", *str));
	    if (_nc_waddch_nosync(win, (chtype)*str++) == ERR) {
	      code = ERR;
	      break;
	    }
	  }
	  _nc_synchook(win);
	}
	TR(TRACE_VIRTPUT, ("waddnstr returns %d", code));
	returnCode(code);
}

int
waddchnstr(WINDOW *win, const chtype *const astr, int n)
{
short y = win->_cury;
short x = win->_curx;
int code = OK;

	T((T_CALLED("waddchnstr(%p,%p,%d)"), win, astr, n));

	if (!win)
	  returnCode(ERR);

	if (n < 0) {
		const chtype *str;
		n = 0;
		for (str=(const chtype *)astr; *str!=0; str++)
			n++;
	}
	if (n > win->_maxx - x + 1)
		n = win->_maxx - x + 1;
	if (n == 0)
		returnCode(code);

	if (win->_line[y].firstchar == _NOCHANGE)
	{
		win->_line[y].firstchar = x;
		win->_line[y].lastchar = x+n-1;
	}
	else
	{
		if (x < win->_line[y].firstchar)
			win->_line[y].firstchar = x;
		if (x+n-1 > win->_line[y].lastchar)
			win->_line[y].lastchar = x+n-1;
	}
	
	memcpy(win->_line[y].text+x, astr, n*sizeof(*astr));

	_nc_synchook(win);
	returnCode(code);
}
