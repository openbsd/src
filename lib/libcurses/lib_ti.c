/*	$OpenBSD: lib_ti.c,v 1.2 1998/10/31 06:30:30 millert Exp $	*/

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
 *  Author: Zeyd M. Ben-Halim <zmbenhal@netcom.com> 1992,1995               *
 *     and: Eric S. Raymond <esr@snark.thyrsus.com>                         *
 ****************************************************************************/


#include <curses.priv.h>

#include <term.h>
#include <tic.h>

MODULE_ID("$From: lib_ti.c,v 1.12 1998/09/26 12:26:38 tom Exp $")

int tigetflag(NCURSES_CONST char *str)
{
int i;

	T(("tigetflag(%s)", str));

	if (cur_term != 0) {
		for (i = 0; i < BOOLCOUNT; i++) {
			if (!strcmp(str, boolnames[i])) {
				/* setupterm forces invalid booleans to false */
				return cur_term->type.Booleans[i];
			}
		}
	}

	return ABSENT_BOOLEAN;
}

int tigetnum(NCURSES_CONST char *str)
{
int i;

	T(("tigetnum(%s)", str));

	if (cur_term != 0) {
		for (i = 0; i < NUMCOUNT; i++) {
			if (!strcmp(str, numnames[i])) {
				if (!VALID_NUMERIC(cur_term->type.Numbers[i]))
					return -1;
				return cur_term->type.Numbers[i];
			}
		}
	}

	return CANCELLED_NUMERIC;	/* Solaris returns a -1 instead */
}

char *tigetstr(NCURSES_CONST char *str)
{
int i;

	T(("tigetstr(%s)", str));

	if (cur_term != 0) {
		for (i = 0; i < STRCOUNT; i++) {
			if (!strcmp(str, strnames[i])) {
				/* setupterm forces cancelled strings to null */
				return cur_term->type.Strings[i];
			}
		}
	}

	return CANCELLED_STRING;
}
