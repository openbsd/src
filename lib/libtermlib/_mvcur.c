/*	$OpenBSD: _mvcur.c,v 1.1 1996/10/12 03:08:25 tholo Exp $	*/

/*
 * Copyright (c) 1996 SigmaSoft, Th. Lockert <tholo@sigmasoft.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by SigmaSoft, Th.  Lockert.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef lint
static char rcsid[] = "$OpenBSD: _mvcur.c,v 1.1 1996/10/12 03:08:25 tholo Exp $";
#endif

#include <string.h>
#include <unistd.h>
#include <termios.h>
#include "term.h"

static int
rawmode()
{
    struct termios ti;

    if (tcgetattr(STDIN_FILENO, &ti) < 0)
	return 1;
    if (ti.c_oflag & OPOST)
	if (ti.c_oflag & ONLCR)
	    return 0;
    return 1;
}

/*
 * Optimized cursor movement, assume cursor is currently
 * located at (oldx,oldy), output what is needed for the
 * cursor to be relocated to (newx,newy)
 */
int
_mvcur(oldy, oldx, newy, newx)
    int oldy;
    int oldx;
    int newy;
    int newx;
{
    int l, c, raw;
    char *p;

    if (newx >= columns) {
	newy += newx / columns;
	newx %= columns;
    }
    if (oldx >= columns) {
	l = (oldx + 1) / columns;
	oldy += l;
	oldx %= columns;
	if (!auto_right_margin) {
	    raw = rawmode();
	    while (l > 0) {
		if (raw)
		    if (carriage_return != NULL)
			tputs(carriage_return, 0, _ti_outc);
		    else
			_ti_outc('\r');
		if (linefeed_if_not_lf != NULL)
		    tputs(linefeed_if_not_lf, 0, _ti_outc);
		else
		    _ti_outc('\n');
		l--;
	    }
	    oldx = 0;
	}
	if (oldy >= lines - 1) {
	    newy -= oldy - (lines - 1);
	    oldy = lines - 1;
	}
    }
    if (newy >= lines) {
	l = newy;
	newy = lines - 1;
	if (oldy < lines - 1) {
	    c = newx;
	    if (cursor_address == NULL)
		newx = 0;
	    mvcur(oldy, oldx, newy, newx);
	    newx = c;
	}
	while (l >= lines) {
	    if (linefeed_if_not_lf != NULL)
		tputs(linefeed_if_not_lf, 0, _ti_outc);
	    else
		_ti_outc('\n');
	    l--;
	    oldx = 0;
	}
    }
    if (newy < oldy && !(cursor_address != NULL || cursor_up != NULL))
	newy = oldy;
    if (cursor_address != NULL) {
	p = tparm(cursor_address, newx, newy);
	tputs(p, 0, _ti_outc);
    }
    else
	return ERR;
    return OK;
}
