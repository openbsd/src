
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
**	lib_addch.c
**
**	The routines waddch(), wattr_on(), wattr_off(), wchgat().
**
*/

#include "curses.priv.h"
#include <ctype.h>
#include "unctrl.h"

#define ALL_BUT_COLOR ((chtype)~(A_COLOR))

int wattr_on(WINDOW *win, const attr_t at)
{
	T(("wattr_on(%p,%s) current = %s", win, _traceattr(at), _traceattr(win->_attrs)));
	if (PAIR_NUMBER(at) > 0x00) {
		win->_attrs = (win->_attrs & ALL_BUT_COLOR) | at ;
		T(("new attribute is %s", _traceattr(win->_attrs)));
	} else {
		win->_attrs |= at;
		T(("new attribute is %s", _traceattr(win->_attrs)));
	}
	return OK;
}

int wattr_off(WINDOW *win, const attr_t at)
{
#define IGNORE_COLOR_OFF FALSE

	T(("wattr_off(%p,%s) current = %s", win, _traceattr(at), _traceattr(win->_attrs)));
	if (IGNORE_COLOR_OFF == TRUE) {
		if (PAIR_NUMBER(at) == 0xff) /* turn off color */
			win->_attrs &= ~at;
		else /* leave color alone */
			win->_attrs &= ~(at|ALL_BUT_COLOR);
	} else {
		if (PAIR_NUMBER(at) > 0x00) /* turn off color */
			win->_attrs &= ~at;
		else /* leave color alone */
			win->_attrs &= ~(at|ALL_BUT_COLOR);
	}
	T(("new attribute is %s", _traceattr(win->_attrs)));
	return OK;
}

int wchgat(WINDOW *win, int n, attr_t attr, short color, const void *opts)
{
    int	i;

    for (i = win->_curx; i <= win->_maxx && (n == -1 || (n-- > 0)); i++)
	win->_line[win->_cury].text[i]
	    = (win->_line[win->_cury].text[i] & A_CHARTEXT)
		| attr
		| COLOR_PAIR(color);

    return OK;
}

/*
 * Ugly microtweaking alert.  Everything from here to end of module is
 * likely to be speed-critical -- profiling data sure says it is!
 * Most of the important screen-painting functions are shells around
 * waddch().  So we make every effort to reduce function-call overhead
 * by inlining stuff, even at the cost of making wrapped copies for
 * export.  Also we supply some internal versions that don't call the
 * window sync hook, for use by string-put functions.
 */

static __inline chtype render_char(WINDOW *win, chtype oldch, chtype newch)
/* compute a rendition of the given char correct for the current context */
{
	if ((oldch & A_CHARTEXT) == ' ')
		newch |= win->_bkgd;
	else if (!(newch & A_ATTRIBUTES))
		newch |= (win->_bkgd & A_ATTRIBUTES);
	TR(TRACE_VIRTPUT, ("bkg = %lx -> ch = %lx", win->_bkgd, newch));

	return(newch);
}

chtype _nc_render(WINDOW *win, chtype oldch, chtype newch)
/* make render_char() visible while still allowing us to inline it below */
{
    return(render_char(win, oldch, newch));
}

/* actions needed to process a newline within addch_nosync() */
#define DO_NEWLINE	x = 0; \
			win->_flags &= ~_NEED_WRAP; \
			y++; \
			if (y > win->_regbottom) { \
				y--; \
				if (win->_scroll) \
					scroll(win); \
			}

/* check if position is legal; if not, return error */
#define CHECK_POSITION(win, x, y) \
	if (y > win->_maxy || x > win->_maxx || y < 0 || x < 0) { \
		TR(TRACE_VIRTPUT, ("Alert! Win=%p _curx = %d, _cury = %d " \
				   "(_maxx = %d, _maxy = %d)", win, x, y, \
				   win->_maxx, win->_maxy)); \
	  	win->_curx = win->_cury = 0; \
		win->_flags &= ~_NEED_WRAP; \
	    	return(ERR); \
	}

static __inline
int waddch_literal(WINDOW *win, chtype ch)
{
register int x, y;

	x = win->_curx;
	y = win->_cury;

	CHECK_POSITION(win, x, y);

	if (win->_flags & _NEED_WRAP) {
		TR(TRACE_MOVE, ("new char when NEED_WRAP set at %d,%d",y,x));
		DO_NEWLINE
	}

	/*
	 * We used to pass in
	 *	win->_line[y].text[x]
	 * as a second argument, but the value of the old character
	 * is not relevant here.
	 */
	ch = render_char(win, 0, ch);

	TR(TRACE_VIRTPUT, ("win attr = %s", _traceattr(win->_attrs)));
	ch |= win->_attrs;

	if (win->_line[y].text[x] != ch) {
		if (win->_line[y].firstchar == _NOCHANGE)
			win->_line[y].firstchar = win->_line[y].lastchar = x;
		else if (x < win->_line[y].firstchar)
			win->_line[y].firstchar = x;
		else if (x > win->_line[y].lastchar)
			win->_line[y].lastchar = x;

	}

	win->_line[y].text[x++] = ch;
	TR(TRACE_VIRTPUT, ("(%d, %d) = %s | %s", 
			   y, x,
			   _tracechar((unsigned char)(ch & A_CHARTEXT)),
			   _traceattr((ch & (chtype)A_ATTRIBUTES))));
	if (x > win->_maxx) {
		TR(TRACE_MOVE, ("NEED_WRAP set at %d,%d",y,x));
		win->_flags |= _NEED_WRAP;
		x--;
	}

	win->_curx = x;
	win->_cury = y;

	return OK;
}

static __inline
int waddch_nosync(WINDOW *win, const chtype c)
/* the workhorse function -- add a character to the given window */
{
register chtype	ch = c;
register int	x, y;
int		newx;

	x = win->_curx;
	y = win->_cury;

	CHECK_POSITION(win, x, y);

	if (ch & A_ALTCHARSET)
		goto noctrl;

	switch ((int)(ch&A_CHARTEXT)) {
    	case '\t':
		if (win->_flags & _NEED_WRAP) {
		  	x = 0;
			newx = min(TABSIZE, win->_maxx+1);
		} else
			newx = min(x + (TABSIZE-(x%TABSIZE)), win->_maxx+1);
		while (win->_curx < newx) {
	    		if (waddch_literal(win, ' ' | (ch&A_ATTRIBUTES)) == ERR)
				return(ERR);
		}
		return(OK);
    	case '\n':
		wclrtoeol(win);
		DO_NEWLINE
		break;
    	case '\r':
		x = 0;
		win->_flags &= ~_NEED_WRAP;
		break;
    	case '\b':
		if (win->_flags & _NEED_WRAP)
			win->_flags &= ~_NEED_WRAP;
		else if (--x < 0)
			x = 0;
		break;
    	default:
		if (is7bits(ch & A_CHARTEXT) && iscntrl(ch & A_CHARTEXT))
		    	return(waddstr(win, unctrl((unsigned char)ch)));

		/* FALLTHRU */
        noctrl:
		waddch_literal(win, ch);
		return(OK);
	}

	win->_curx = x;
	win->_cury = y;

	return(OK);
}

#undef DO_NEWLINE

int _nc_waddch_nosync(WINDOW *win, const chtype c)
/* export copy of waddch_nosync() so the string-put functions can use it */
{
    return(waddch_nosync(win, c));
}

/*
 * The versions below call _nc_synhook().  We wanted to avoid this in the
 * version exported for string puts; they'll call _nc_synchook once at end
 * of run.
 */

/* These are actual entry points */

int waddch(WINDOW *win, const chtype ch)
{
	TR(TRACE_VIRTPUT, ("waddch(%p, %s | %s) called", win,
			  _tracechar((unsigned char)(ch & A_CHARTEXT)),
			  _traceattr((ch & (chtype)A_ATTRIBUTES))));

	if (waddch_nosync(win, ch) == ERR)
		return(ERR);
	else
	{
		_nc_synchook(win);
		TR(TRACE_VIRTPUT, ("waddch() is done"));
		return(OK);
	}
}

int wechochar(WINDOW *win, const chtype ch)
{
	TR(TRACE_VIRTPUT, ("wechochar(%p,%s (%s)) called", win,
			  _tracechar((unsigned char)(ch & A_CHARTEXT)),
			  _traceattr((ch & (chtype)A_ATTRIBUTES))));

	if (waddch_literal(win, ch) == ERR)
		return(ERR);
	else
	{
		_nc_synchook(win);
		TR(TRACE_VIRTPUT, ("wechochar() is done"));
		return(OK);
	}
}
