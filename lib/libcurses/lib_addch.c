/*	$OpenBSD: lib_addch.c,v 1.3 1997/12/03 05:21:10 millert Exp $	*/


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
**	The routines waddch(), wchgat().
**
*/

#include <curses.priv.h>
#include <ctype.h>

MODULE_ID("Id: lib_addch.c,v 1.36 1997/10/08 09:38:17 jtc Exp $")

int wchgat(WINDOW *win, int n, attr_t attr, short color, const void *opts GCC_UNUSED)
{
    int	i;

    T((T_CALLED("wchgat(%p,%d,%s,%d)"), win, n, _traceattr(attr), color));

    if (win) {
      toggle_attr_on(attr,COLOR_PAIR(color));

      for (i = win->_curx; i <= win->_maxx && (n == -1 || (n-- > 0)); i++)
	win->_line[win->_cury].text[i]
	  = TextOf(win->_line[win->_cury].text[i]) | attr;

      returnCode(OK);
    }
    else
      returnCode(ERR);
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

/* Return bit mask for clearing color pair number if given ch has color */
#define COLOR_MASK(ch) (~(chtype)((ch)&A_COLOR?A_COLOR:0))

static inline chtype render_char(WINDOW *win, chtype ch)
/* compute a rendition of the given char correct for the current context */
{
	chtype a = win->_attrs;
	
	if (ch == ' ')
	{
		/* color in attrs has precedence over bkgd */
		ch = a | (win->_bkgd & COLOR_MASK(a));
	}
	else
	{
		/* color in attrs has precedence over bkgd */
		a |= (win->_bkgd & A_ATTRIBUTES) & COLOR_MASK(a);
		/* color in ch has precedence */
		ch |= (a & COLOR_MASK(ch));
	}

	TR(TRACE_VIRTPUT, ("bkg = %lx, attrs = %lx -> ch = %lx", win->_bkgd,
		win->_attrs, ch));

	return(ch);
}

chtype _nc_background(WINDOW *win)
/* make render_char() visible while still allowing us to inline it below */
{
	return (win->_bkgd);
}

chtype _nc_render(WINDOW *win, chtype ch)
/* make render_char() visible while still allowing us to inline it below */
{
	return render_char(win, ch);
}

/* check if position is legal; if not, return error */
#ifndef NDEBUG			/* treat this like an assertion */
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
	int x;
	struct ldat *line;

	x = win->_curx;

	CHECK_POSITION(win, x, win->_cury);

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
	TR(TRACE_VIRTPUT, ("win attr = %s", _traceattr(win->_attrs)));

	line = win->_line+win->_cury;
	
	if (line->firstchar == _NOCHANGE)
		line->firstchar = line->lastchar = x;
	else if (x < line->firstchar)
		line->firstchar = x;
	else if (x > line->lastchar)
		line->lastchar = x;

	line->text[x++] = ch;
	
	TR(TRACE_VIRTPUT, ("(%d, %d) = %s", win->_cury, x, _tracechtype(ch)));
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
		if (++win->_cury > win->_regbottom) {
			win->_cury = win->_regbottom;
			win->_curx = win->_maxx;
			if (!win->_scroll)
				return (ERR);
			scroll(win);
		}
		win->_curx = 0;
		return (OK);
	}
	win->_curx = x;
	return OK;
}

static inline
int waddch_nosync(WINDOW *win, const chtype ch)
/* the workhorse function -- add a character to the given window */
{
	int	x, y;
	int	t;
	const char *s;

	if (ch & A_ALTCHARSET)
		return waddch_literal(win, ch);

	t = TextOf(ch);
	s = unctrl(t);
	if (s[1] == 0)	/* not a control char */
		return waddch_literal(win, ch);
	
	x = win->_curx;
	y = win->_cury;

	switch (t) {
	case '\t':
		x += (TABSIZE-(x%TABSIZE));

		/*
		 * Space-fill the tab on the bottom line so that we'll get the
		 * "correct" cursor position.
		 */
		if ((! win->_scroll && (y == win->_regbottom))
		 || (x <= win->_maxx)) {
			chtype blank = (' ' | AttrOf(ch));
			while (win->_curx < x) {
				if (waddch_literal(win, blank) == ERR)
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
		if (x == 0)
			return (OK);
		x--;
		win->_flags &= ~_WRAPPED;
		break;
	default:
		return(waddstr(win, s));
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

	if (win && (waddch_nosync(win, ch) != ERR))
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

	if (win && (waddch_nosync(win, ch) != ERR))
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
