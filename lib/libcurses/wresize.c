/******************************************************************************
 * Copyright 1996,1997 by Thomas E. Dickey <dickey@clark.net>                 *
 * All Rights Reserved.                                                       *
 *                                                                            *
 * Permission to use, copy, modify, and distribute this software and its      *
 * documentation for any purpose and without fee is hereby granted, provided  *
 * that the above copyright notice appear in all copies and that both that    *
 * copyright notice and this permission notice appear in supporting           *
 * documentation, and that the name of the above listed copyright holder(s)   *
 * not be used in advertising or publicity pertaining to distribution of the  *
 * software without specific, written prior permission. THE ABOVE LISTED      *
 * COPYRIGHT HOLDER(S) DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,  *
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO     *
 * EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY         *
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER       *
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF       *
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN        *
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.                   *
 ******************************************************************************/

#include <curses.priv.h>
#include <term.h>

MODULE_ID("Id: wresize.c,v 1.5 1997/02/01 23:22:54 tom Exp $")

/*
 * Reallocate a curses WINDOW struct to either shrink or grow to the specified
 * new lines/columns.  If it grows, the new character cells are filled with
 * blanks.  The application is responsible for repainting the blank area.
 */

static void *doalloc(void *p, size_t n)
{
	if (p == 0)
		p = malloc(n);
	else
		p = realloc(p, n);
	return p;
}

#define DOALLOC(p,t,n)  (t *)doalloc(p, sizeof(t)*(n))
#define	ld_ALLOC(p,n)	DOALLOC(p,struct ldat,n)
#define	c_ALLOC(p,n)	DOALLOC(p,chtype,n)

int
wresize(WINDOW *win, int ToLines, int ToCols)
{
	register int	row;
	int	size_x, size_y;
	struct ldat *pline = (win->_flags & _SUBWIN) ? win->_parent->_line : 0;

#ifdef TRACE
	T((T_CALLED("wresize(%p,%d,%d)"), win, ToLines, ToCols));
	TR(TRACE_UPDATE, ("...beg (%d, %d), max(%d,%d), reg(%d,%d)",
		win->_begy, win->_begx,
		win->_maxy, win->_maxx,
		win->_regtop, win->_regbottom));
	if (_nc_tracing & TRACE_UPDATE)
		_tracedump("...before", win);
#endif

	if (--ToLines < 0 || --ToCols < 0)
		returnCode(ERR);

	size_x = win->_maxx;
	size_y = win->_maxy;

	if (ToLines == size_y
	 && ToCols  == size_x)
		returnCode(OK);

	/*
	 * If the number of lines has changed, adjust the size of the overall
	 * vector:
	 */
	if (ToLines != size_y) {
		if (! (win->_flags & _SUBWIN)) {
			for (row = ToLines+1; row <= size_y; row++)
				free((char *)(win->_line[row].text));
		}

		win->_line = ld_ALLOC(win->_line, ToLines+1);
		if (win->_line == 0)
			returnCode(ERR);

		for (row = size_y+1; row <= ToLines; row++) {
			win->_line[row].text      = 0;
			win->_line[row].firstchar = 0;
			win->_line[row].lastchar  = ToCols;
			if ((win->_flags & _SUBWIN)) {
				win->_line[row].text =
	    			&pline[win->_begy + row].text[win->_begx];
			}
		}
	}

	/*
	 * Adjust the width of the columns:
	 */
	for (row = 0; row <= ToLines; row++) {
		chtype	*s	= win->_line[row].text;
		int	begin	= (s == 0) ? 0 : size_x + 1;
		int	end	= ToCols;
		chtype	blank	= _nc_background(win);

		win->_line[row].oldindex = row;

		if (ToCols != size_x || s == 0) {
			if (! (win->_flags & _SUBWIN)) {
				win->_line[row].text = s = c_ALLOC(s, ToCols+1);
				if (win->_line[row].text == 0)
					returnCode(ERR);
			} else if (s == 0) {
				win->_line[row].text = s =
	    			&pline[win->_begy + row].text[win->_begx];
			}

			if (end >= begin) {	/* growing */
				if (win->_line[row].firstchar < begin)
					win->_line[row].firstchar = begin;
				win->_line[row].lastchar = ToCols;
				do {
					s[end] = blank;
				} while (--end >= begin);
			} else {		/* shrinking */
				win->_line[row].firstchar = 0;
				win->_line[row].lastchar  = ToCols;
			}
		}
	}

	/*
	 * Finally, adjust the parameters showing screen size and cursor
	 * position:
	 */
	win->_maxx = ToCols;
	win->_maxy = ToLines;

	if (win->_regtop > win->_maxy)
		win->_regtop = win->_maxy;
	if (win->_regbottom > win->_maxy
	 || win->_regbottom == size_y)
		win->_regbottom = win->_maxy;

	if (win->_curx > win->_maxx)
		win->_curx = win->_maxx;
	if (win->_cury > win->_maxy)
		win->_cury = win->_maxy;

#ifdef TRACE
	TR(TRACE_UPDATE, ("...beg (%d, %d), max(%d,%d), reg(%d,%d)",
		win->_begy, win->_begx,
		win->_maxy, win->_maxx,
		win->_regtop, win->_regbottom));
	if (_nc_tracing & TRACE_UPDATE)
		_tracedump("...after:", win);
#endif
	returnCode(OK);
}
