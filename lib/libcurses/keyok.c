/*	$OpenBSD: keyok.c,v 1.1 1997/12/03 05:21:09 millert Exp $	*/

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

MODULE_ID("Id: keyok.c,v 1.1 1997/05/29 01:32:51 tom Exp $")

/*
 * Enable (or disable) ncurses' interpretation of a keycode by adding (or
 * removing) the corresponding 'tries' entry.
 *
 * Do this by storing a second tree of tries, which records the disabled keys. 
 * The simplest way to copy is to make a function that returns the string (with
 * nulls set to 0200), then use that to reinsert the string into the
 * corresponding tree.
 */

int keyok(int c, bool flag)
{
	int code = ERR;
	char *s;

	T((T_CALLED("keyok(%d,%d)"), c, flag));
	if (flag) {
		if ((s = _nc_expand_try(SP->_key_ok, c, 0)) != 0
		 && _nc_remove_key(&(SP->_key_ok), c)) {
			_nc_add_to_try(&(SP->_keytry), s, c);
			free(s);
			code = OK;
		}
	} else {
		if ((s = _nc_expand_try(SP->_keytry, c, 0)) != 0
		 && _nc_remove_key(&(SP->_keytry), c)) {
			_nc_add_to_try(&(SP->_key_ok), s, c);
			free(s);
			code = OK;
		}
	}
	returnCode(code);
}
