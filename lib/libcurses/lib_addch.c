
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

#include <curses.priv.h>
#include <ctype.h>

MODULE_ID("Id: lib_addch.c,v 1.30 1997/04/12 17:45:55 tom Exp $")

int wattr_on(WINDOW *win, const attr_t at)
{
	T((T_CALLED("wattr_on(%p,%s)"), win, _traceattr(at)));
	T(("... current %s", _traceattr(win->_attrs)));
	toggle_attr_on(win->_attrs,at);
	returnCode(OK);
}

int wattr_off(WINDOW *win, const attr_t at)
{
	T((T_CALLED("wattr_off(%p,%s)"), win, _traceattr(at)));
	T(("... current %s", _traceattr(win->_attrs)));
	toggle_attr_off(win->_attrs,at);
	returnCode(OK);
}

int wchgat(WINDOW *win, int n, attr_t attr, short color, const void *opts GCC_UNUSED)
{
    int	i;

    T((T_CALLED("wchgat(%p,%d,%s,%d)"), win, n, _traceattr(attr), color));

    toggle_attr_on(attr,COLOR_PAIR(color));

    for (i = win->_curx; i <= win->_maxx && (n == -1 || (n-- > 0)); i++)
	win->_line[win->_cury].text[i]
	    = ch_or_attr(TextOf(win->_line[win->_cury].text[i]),attr);

    returnCode(OK);
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

static inline chtype render_char(WINDOW *win, chtype ch)
/* compute a rendition of the given char correct for the current context */
{
	if (TextOf(ch) == ' ')
		ch = ch_or_attr(ch, win->_bkgd);
	else if (!(ch & A_ATTRIBUTES))
		ch = ch_or_attr(ch, (win->_bkgd & A_ATTRIBUTES));
	TR(TRACE_VIRTPUT, ("bkg = %#lx -> ch = %#lx", win->_bkgd, ch));

	return(ch);
}

chtype _nc_background(WINDOW *win)
/* make render_char() visible while still allowing us to inline it below */
{
    return(render_char(win, BLANK));
}

chtype _nc_render(WINDOW *win, chtype ch)
/* make render_char() visible while still allowing us to inline it below */
{
    chtype c = render_char(win,ch);
    return (ch_or_attr(c,win->_attrs));
}

/* check if position is legal; if not, return error */
#ifdef NDEBUG			/* treat this like an assertion */
#define CHECK_POSITION(win, x, y) \
	if (y > win->_maxy \
	 || x > win->_maxx \
	 || y < 0 \
	 || x < 0) { \
		TR(TRACE_VIRTPUT, ("Alert! Win=%p _curx = %d, _cury = %d " \
				   "(_maxx = %d, _maxy = %d)", win, x, y, \
				   win->_maxx, win->_maxy)); \
		return(ERR); \
	}
#else
#define CHECK_POSITION(win, x, y) /* nothing */
#endif

static inline
int waddch_literal(WINDOW *win, chtype ch)
{
register int x, y;

	x = win->_curx;
	y = win->_cury;

	CHECK_POSITION(win, x, y);

	/*
	 * If we're trying to add a character at the lower-right corner more
	 * than once, fail.  (Moving the cursor will clear the flag).
	 */
	if (win->_flags & _WRAPPED) {
		if (x >= win->_maxx)
			return (ERR);
		win->_flags &= ~_WRAPPED;
	}

	ch = render_char(win, ch);
	ch = ch_or_attr(ch,win->_attrs);
	TR(TRACE_VIRTPUT, ("win attr = %s", _traceattr(win->_attrs)));

	if (win->_line[y].text[x] != ch) {
		if (win->_line[y].firstchar == _NOCHANGE)
			win->_line[y].firstchar = win->_line[y].lastchar = x;
		else if (x < win->_line[y].firstchar)
			win->_line[y].firstchar = x;
		else if (x > win->_line[y].lastchar)
			win->_line[y].lastchar = x;

	}

	win->_line[y].text[x++] = ch;
	TR(TRACE_VIRTPUT, ("(%d, %d) = %s", y, x, _tracechtype(ch)));
	if (x > win->_maxx) {
		/*
		 * The _WRAPPED flag is useful only for telling an application
		 * that we've just wrapped the cursor.  We don't do anything
		 * with this flag except set it when wrapping, and clear it
		 * whenever we move the cursor.  If we try to wrap at the
		 * lower-right corner of a window, we cannot move the cursor
		 * (since that wouldn't be legal).  So we return an error
		 * (which is what SVr4 does).  Unlike SVr4, we can successfully
		 * add a character to the lower-right corner.
		 */
		win->_flags |= _WRAPPED;
		if (++y > win->_regbottom) {
			y = win->_regbottom;
			x = win->_maxx;
			if (win->_scroll)
				scroll(win);
			else {
				win->_curx = x;
				win->_cury = y;
				return (ERR);
			}
		}
		x = 0;
	}

	win->_curx = x;
	win->_cury = y;

	return OK;
}

static inline
int waddch_nosync(WINDOW *win, const chtype c)
/* the workhorse function -- add a character to the given window */
{
register chtype	ch = c;
register int	x, y;

	x = win->_curx;
	y = win->_cury;

	CHECK_POSITION(win, x, y);

	if (ch & A_ALTCHARSET)
		goto noctrl;

	switch ((int)TextOf(ch)) {
	case '\t':
		x += (TABSIZE-(x%TABSIZE));

		/*
		 * Space-fill the tab on the bottom line so that we'll get the
		 * "correct" cursor position.
		 */
		if ((! win->_scroll && (y == win->_regbottom))
		 || (x <= win->_maxx)) {
			while (win->_curx < x) {
				if (waddch_literal(win, (' ' | AttrOf(ch))) == ERR)
					return(ERR);
			}
			break;
		} else {
			wclrtoeol(win);
			win->_flags |= _WRAPPED;
			if (++y > win->_regbottom) {
				x = win->_maxx;
				y--;
				if (win->_scroll) {
					scroll(win);
					x = 0;
				}
			} else {
				x = 0;
			}
		}
		break;
	case '\n':
		wclrtoeol(win);
		if (++y > win->_regbottom) {
			y--;
			if (win->_scroll)
				scroll(win);
			else
				return (ERR);
		}
		/* FALLTHRU */
	case '\r':
		x = 0;
		win->_flags &= ~_WRAPPED;
		break;
	case '\b':
		if (x > 0) {
			x--;
			win->_flags &= ~_WRAPPED;
		}
		break;
	default:
		if (is7bits(TextOf(ch)) && iscntrl(TextOf(ch)))
			return(waddstr(win, unctrl((unsigned char)ch)));

		/* FALLTHRU */
	noctrl:
		return waddch_literal(win, ch);
	}

	win->_curx = x;
	win->_cury = y;

	return(OK);
}

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
	int code = ERR;

	TR(TRACE_VIRTPUT|TRACE_CCALLS, (T_CALLED("waddch(%p, %s)"), win, _tracechtype(ch)));

	if (waddch_nosync(win, ch) != ERR)
	{
		_nc_synchook(win);
		code = OK;
	}

	TR(TRACE_VIRTPUT|TRACE_CCALLS, (T_RETURN("%d"), code));
	return(code);
}

int wechochar(WINDOW *win, const chtype ch)
{
	int code = ERR;

	TR(TRACE_VIRTPUT|TRACE_CCALLS, (T_CALLED("wechochar(%p, %s)"), win, _tracechtype(ch)));

	if (waddch_literal(win, ch) != ERR)
	{
		bool	save_immed = win->_immed;
		win->_immed = TRUE;
		_nc_synchook(win);
		win->_immed = save_immed;
		code = OK;
	}
	TR(TRACE_VIRTPUT|TRACE_CCALLS, (T_RETURN("%d"), code));
	return(code);
}
