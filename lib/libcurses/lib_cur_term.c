/*	$OpenBSD: lib_cur_term.c,v 1.2 1998/10/31 06:30:29 millert Exp $	*/

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

MODULE_ID("$From: lib_cur_term.c,v 1.3 1998/09/19 19:21:05 Alexander.V.Lukyanov Exp $")

TERMINAL *cur_term;

int _nc_get_curterm(TTY *buf)
{
	if (cur_term == 0
	 || GET_TTY(cur_term->Filedes, buf) != 0)
		return(ERR);
	return (OK);
}

int _nc_set_curterm(TTY *buf)
{
	if (cur_term == 0
	 || SET_TTY(cur_term->Filedes, buf) != 0)
		return(ERR);
	return (OK);
}

TERMINAL *set_curterm(TERMINAL *term)
{
	TERMINAL *oldterm = cur_term;

	cur_term = term;
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
