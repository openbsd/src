/*	$OpenBSD: lib_redrawln.c,v 1.1 1997/12/03 05:21:28 millert Exp $	*/

/******************************************************************************
 * Copyright 1997 by Thomas E. Dickey <dickey@clark.net>                      *
 * All Rights Reserved.                                                       *
 *                                                                            *
 * Permission to use, copy, modify, and distribute this software and its      *
 * documentation for any purpose and without fee is hereby granted, provided  *
 * that the above copyright notice appear in all copies and that both that    *
 * copyright notice and this permission notice appear in supporting           *
 * documentation, and that the name of the above listed copyright holder(s)   *
 * not be used in advertising or publicity pertaining to distribution of the  *
 * software without specific, written prior permission.                       *
 *                                                                            *
 * THE ABOVE LISTED COPYRIGHT HOLDER(S) DISCLAIM ALL WARRANTIES WITH REGARD   *
 * TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND  *
 * FITNESS, IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE  *
 * FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES          *
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN      *
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR *
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.                *
 ******************************************************************************/

/*
 *	lib_redrawln.c
 *
 *	The routine wredrawln().
 *
 */

#include <curses.priv.h>

MODULE_ID("Id: lib_redrawln.c,v 1.1 1997/11/29 20:10:56 tom Exp $")

int wredrawln(WINDOW *win, int beg, int num)
{
int i;

	T((T_CALLED("wredrawln(%p,%d,%d)"), win, beg, num));

	if (touchline(win, beg, num) == OK) {
		size_t len = win->_maxx * sizeof(chtype);

		/*
		 * XSI says that wredrawln() tells the library not to base
		 * optimization on the contents of the lines that are marked.
		 * We do that by changing the contents to nulls after touching
		 * the corresponding lines to get the optimizer's attention.
		 *
		 * FIXME: this won't work if the application makes further
		 * updates before the next refresh.
		 */
		for (i = beg; (i < beg + num) && (i < win->_maxy); i++) {
			memset(win->_line[i].text, 0, len);
		}
	}
	returnCode(OK);
}
