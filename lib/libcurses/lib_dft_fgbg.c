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
#include <term.h>

MODULE_ID("Id: lib_dft_fgbg.c,v 1.2 1997/02/02 01:45:36 tom Exp $")

/*
 * Modify the behavior of color-pair 0 so that the library doesn't assume that
 * it is black on white.  This is an extension to XSI curses.
 *
 * Invoke this function after 'start_color()'.
 */
int
use_default_colors(void)
{
	T((T_CALLED("use_default_colors()")));

	if (!SP->_coloron)
		returnCode(ERR);

	if (!orig_pair && !orig_colors)
		returnCode(ERR);

	if (initialize_pair)	/* don't know how to handle this */
		returnCode(ERR);

	SP->_default_color = TRUE;
	SP->_color_pairs[0] = PAIR_OF(C_MASK, C_MASK);
	returnCode(OK);
}
