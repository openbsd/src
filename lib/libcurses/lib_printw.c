/*	$OpenBSD: lib_printw.c,v 1.5 1997/12/03 05:21:27 millert Exp $	*/

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
**	lib_printw.c
**
**	The routines printw(), wprintw() and friends.
**
*/

#include <curses.priv.h>

MODULE_ID("Id: lib_printw.c,v 1.5 1997/08/31 21:22:33 tom Exp $")

int printw(const char *fmt, ...)
{
	va_list argp;
	int code;

	T(("printw(%s,...) called", _nc_visbuf(fmt)));

	va_start(argp, fmt);
	code = vwprintw(stdscr, fmt, argp);
	va_end(argp);

	return code;
}

int wprintw(WINDOW *win, const char *fmt, ...)
{
	va_list argp;
	int code;

	T(("wprintw(%p,%s,...) called", win, _nc_visbuf(fmt)));

	va_start(argp, fmt);
	code = vwprintw(win, fmt, argp);
	va_end(argp);

	return code;
}

int mvprintw(int y, int x, const char *fmt, ...)
{
	va_list argp;
	int code = move(y, x);

	if (code != ERR) {
		va_start(argp, fmt);
		code = vwprintw(stdscr, fmt, argp);
		va_end(argp);
	}
	return code;
}

int mvwprintw(WINDOW *win, int y, int x, const char *fmt, ...)
{
	va_list argp;
	int code = wmove(win, y, x);

	if (code != ERR) {
		va_start(argp, fmt);
		code = vwprintw(win, fmt, argp);
		va_end(argp);
	}
	return code;
}

int vwprintw(WINDOW *win, const char *fmt, va_list argp)
{
	char *buf = _nc_printf_string(fmt, argp);
	int code = ERR;

	if (buf != 0) {
		code = waddstr(win, buf);
#if USE_SAFE_SPRINTF
		free(buf);
#endif
	}
	return code;
}
