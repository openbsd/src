/*	$OpenBSD: lib_cur_term.c,v 1.1 1999/01/18 19:10:17 millert Exp $	*/

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
/*
 * Module that "owns" the 'cur_term' variable:
 *
 *	TERMINAL *set_curterm(TERMINAL *)
 *	int del_curterm(TERMINAL *)
 */

#include <curses.priv.h>
#include <term.h>	/* TTY, cur_term */
#include <termcap.h>	/* ospeed */

MODULE_ID("$From: lib_cur_term.c,v 1.6 1999/01/10 00:48:44 tom Exp $")

TERMINAL *cur_term;

TERMINAL *set_curterm(TERMINAL *term)
{
	TERMINAL *oldterm = cur_term;

	if ((cur_term = term) != 0) {
		ospeed = _nc_ospeed(cur_term->_baudrate);
		PC = (pad_char != NULL) ? pad_char[0] : 0; 
	}
	return oldterm;
}

int del_curterm(TERMINAL *term)
{
	T((T_CALLED("del_curterm(%p)"), term));

	if (term != 0) {
		FreeIfNeeded(term->type.str_table);
		FreeIfNeeded(term->type.term_names);
		free(term);
		if (term == cur_term)
			cur_term = 0;
		returnCode(OK);
	}
	returnCode(ERR);
}
