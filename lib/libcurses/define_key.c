/*	$OpenBSD: define_key.c,v 1.1 1997/12/03 05:21:08 millert Exp $	*/

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

MODULE_ID("Id: define_key.c,v 1.1 1997/05/29 09:56:26 tom Exp $")

int
define_key(char *str, int keycode)
{
	int code = ERR;

	T((T_CALLED("define_key(%s,%d)"), _nc_visbuf(str), keycode));
	if (keycode > 0) {
		if (has_key(keycode)) {
			if (_nc_remove_key(&(SP->_keytry), keycode))
				code = OK;
		}
		if (str != 0) {
			(void) _nc_add_to_try(&(SP->_keytry), str, keycode);
			code = OK;
		}
	}
	returnCode(code);
}
