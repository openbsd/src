/*	$OpenBSD: softscroll.c,v 1.2 1998/07/23 21:20:04 millert Exp $	*/

/****************************************************************************
 * Copyright (c) 1998 Free Software Foundation, Inc.                        *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/

/****************************************************************************
 *  Author: Thomas E. Dickey <dickey@clark.net> 1997                        *
 ****************************************************************************/
#include <curses.priv.h>
#include <term.h>

MODULE_ID("$From: softscroll.c,v 1.6 1998/02/11 12:13:56 tom Exp $")

/*
 * Compute indices for the given WINDOW, preparing it for scrolling.
 *
 * TODO (this implementation is only for proof-of-concept)
 *	(a) ensure that curscr's oldindex values are cached properly so we
 *	    don't have to recompute them on each pass.
 *	(b) investigate if there are gains to be made by iterating newscr's
 *	    row indices outward from the current position, rather than by
 *	    all rows.
 */
static void compute_curscr(void)
{
	int y, x, z;
	for (y = 0; y < screen_lines; y++) {
		struct ldat *nline = &curscr->_line[y];
		int found = y;
		for (z = 0; z < y; z++) {
			int same = TRUE;
			struct ldat *oline = &curscr->_line[z];
			for (x = 0; x < screen_columns; x++) {
				if (nline->text[x] != oline->text[x]) {
					same = FALSE;
					break;
				}
			}
			if (same) {
				found = z;
				break;
			}
		}
		nline->oldindex = found;
	}
}

static void compute_newscr(void)
{
	int y, x, z;
	for (y = 0; y < screen_lines; y++) {
		struct ldat *nline = &newscr->_line[y];
		int found = _NEWINDEX;
		for (z = 0; z < screen_lines; z++) {
			int same = TRUE;
			struct ldat *oline = &curscr->_line[z];
			for (x = 0; x < screen_columns; x++) {
				if (nline->text[x] != oline->text[x]) {
					same = FALSE;
					break;
				}
			}
			if (same) {
				found = z;
				break;
			}
		}
		nline->oldindex = found;
	}
}

void
_nc_setup_scroll(void)
{
#ifdef TRACE
	if (_nc_tracing & TRACE_UPDATE) {
		_tracef("_nc_setup_scroll");
		_nc_linedump();
	}
#endif /* TRACE */
	compute_curscr();
	compute_newscr();
#ifdef TRACE
	if (_nc_tracing & TRACE_UPDATE) {
		_tracef("..._nc_setup_scroll");
		_nc_linedump();
	}
#endif
}

#define MINDISP		2
#define NEWNUM(n)	newscr->_line[n].oldindex
#define OLDNUM(n)	curscr->_line[n].oldindex

/*
 * This performs essentially the same function as _nc_scroll_optimize(), but
 * uses different assumptions about the .oldindex values.  More than one line
 * may have the same .oldindex value.  We don't assume the values are ordered.
 *
 * (Neither algorithm takes into account the cost of constructing the lines
 * which are scrolled)
 */
void
_nc_perform_scroll(void)
{
	int disp;
	int row;
	int top, bottom, maxdisp;
	int partial;

	/*
	 * Find the top/bottom lines that are different between curscr and
	 * newscr, limited by the terminal's ability to scroll portions of the
	 * screen.
	 *
	 * FIXME: this doesn't account for special cases of insert/delete line.
	 */
	if (change_scroll_region
	 && (scroll_forward || parm_index)
	 && (scroll_reverse || parm_rindex)) {
		partial = TRUE;
		for (row = 0, top = -1; row < screen_lines; row++) {
			if (OLDNUM(row) != NEWNUM(row)) {
				break;
			}
			top = row;
		}
		top++;

		for (row = screen_lines-1, bottom = screen_lines; row >= 0; row--) {
			if (OLDNUM(row) != NEWNUM(row)) {
				break;
			}
			bottom = row;
		}
		bottom--;
	} else {
		partial = FALSE;
		top     = 0;
		bottom  = screen_lines - 1;
	}

	maxdisp = (bottom - top + 1) / 2;
	if (maxdisp < MINDISP)
		return;

	T(("_nc_perform_scroll %d..%d (maxdisp=%d)", top, bottom, maxdisp));

	for (disp = 1; disp < maxdisp; disp++) {
		int n;
		int fn, fwd = 0;
		int bn, bak = 0;
		int first, last;
		int moved;

		do {
			/* check for forward-movement */
			for (fn = top + disp; fn < screen_lines - disp; fn++) {
				int eql = 0;
				for (n = fn, fwd = 0; n < screen_lines; n++) {
					if (NEWNUM(n) == _NEWINDEX
					 || NEWNUM(n) != OLDNUM(n-disp))
						break;
					fwd++;
					if (OLDNUM(n) == NEWNUM(n))
						eql++;
				}
				if (eql == fwd)
					fwd = 0;
				if (fwd >= disp)
					break;
				fwd = 0;
			}

			/* check for backward-movement */
			for (bn = top + disp; bn < screen_lines - disp; bn++) {
				int eql = 0;
				for (n = bn, bak = 0; n < screen_lines; n++) {
					if (OLDNUM(n) == _NEWINDEX
					 || OLDNUM(n) != NEWNUM(n-disp))
						break;
					bak++;
					if (OLDNUM(n-disp) == NEWNUM(n-disp))
						eql++;
				} 
				if (eql == bak)
					bak = 0;
				if (bak >= disp)
					break;
				bak = 0;
			}

			/* choose only one, in case they overlap */
			if (fwd > bak) {
				first = fn - disp;
				last  = fn + fwd - 1;
				moved = -disp;
			} else if (bak) {
				first = bn - disp;
				last  = bn + bak - 1;
				moved = disp;
			} else {
				break;
			}

			TR(TRACE_UPDATE | TRACE_MOVE, ("scroll [%d, %d] by %d", first, last, moved));
			if (_nc_scrolln(moved, first, last, screen_lines - 1) == ERR)
			{
				TR(TRACE_UPDATE | TRACE_MOVE, ("unable to scroll"));
				break;
			}

			/* If the scrolled text was at one end of the range
			 * of changed lines, adjust the loop limits.
			 */
			if (first == top)
				top = last + 1;
			if (last == bottom)
				bottom = first - 1;

			maxdisp = (bottom - top + 1) / 2;
			if (maxdisp < MINDISP)
				return;

			/* In any case, mark the lines so we don't try to
			 * use them in a subsequent scroll.
			 */
			for (fn = first; fn <= last; fn++) {
				OLDNUM(fn) =
				NEWNUM(fn) = _NEWINDEX;
			}
		} while (partial);
	}
}
