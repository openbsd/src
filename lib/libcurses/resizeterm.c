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

/*
 * This is an extension to the curses library.  It provides callers with a hook
 * into the NCURSES data to resize windows, primarily for use by programs
 * running in an X Window terminal (e.g., xterm).  I abstracted this module
 * from my application library for NCURSES because it must be compiled with
 * the private data structures -- T.Dickey 1995/7/4.
 */

#include <curses.priv.h>
#include <term.h>

MODULE_ID("Id: resizeterm.c,v 1.3 1997/02/02 01:03:06 tom Exp $")

/*
 * This function reallocates NCURSES window structures.  It is invoked in
 * response to a SIGWINCH interrupt.  Other user-defined windows may also need
 * to be reallocated.
 *
 * Because this performs memory allocation, it should not (in general) be
 * invoked directly from the signal handler.
 */
int
resizeterm(int ToLines, int ToCols)
{
	int stolen = screen_lines - SP->_lines_avail;
	int bottom = screen_lines + SP->_topstolen - stolen;

	T((T_CALLED("resizeterm(%d,%d) old(%d,%d)"),
		ToLines, ToCols,
		screen_lines, screen_columns));

	if (ToLines != screen_lines
	 || ToCols  != screen_columns) {
		WINDOWLIST *wp;

		for (wp = _nc_windows; wp != 0; wp = wp->next) {
			WINDOW *win = wp->win;
			int myLines = win->_maxy + 1;
			int myCols  = win->_maxx + 1;

			/* pads aren't treated this way */
			if (win->_flags & _ISPAD)
				continue;

			if (win->_begy >= bottom) {
				win->_begy += (ToLines - screen_lines);
			} else {
				if (myLines == screen_lines - stolen
				 && ToLines != screen_lines)
				 	myLines = ToLines - stolen;
				else
				if (myLines == screen_lines
				 && ToLines != screen_lines)
				 	myLines = ToLines;
			}

			if (myCols  == screen_columns
			 && ToCols  != screen_columns)
			 	myCols = ToCols;

			if (wresize(win, myLines, myCols) != OK)
				returnCode(ERR);
		}

		screen_lines   = lines    = ToLines;
		screen_columns = columns  = ToCols;

		SP->_lines_avail = lines - stolen;
	}

	/*
	 * Always update LINES, to allow for call from lib_doupdate.c which
	 * needs to have the count adjusted by the stolen (ripped off) lines.
	 */
	LINES = ToLines - stolen;
	COLS  = ToCols;

	returnCode(OK);
}
