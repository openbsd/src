/*	$OpenBSD: _vidputs.c,v 1.1 1996/09/21 19:22:27 tholo Exp $	*/

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
static char rcsid[] = "$OpenBSD: _vidputs.c,v 1.1 1996/09/21 19:22:27 tholo Exp $";
#endif

#include <stdlib.h>
#include "term.h"

int
_vidputs(attr, outc)
    chtype attr;
    int (*outc) __P((int));
{
    static chtype	old_attr;
    chtype		attr_on, attr_off;

    if (attr == old_attr)
	return OK;

    attr_on = (~attr & old_attr) & (chtype)(~A_COLOR);
    attr_off = (attr & ~old_attr) & (chtype)(~A_COLOR);

    if (attr == A_NORMAL) {
	if ((old_attr & A_ALTCHARSET) && exit_alt_charset_mode != NULL) {
	    tputs(exit_alt_charset_mode, 1, outc);
	    old_attr &= ~A_ALTCHARSET;
	}
	if (old_attr)
	    tputs(exit_attribute_mode, 1, outc);
    }
    else if (set_attributes) {
	if (attr_on || attr_off) {
	    tputs(tparm(set_attributes,
			attr_on & A_STANDOUT,
			attr_on & A_UNDERLINE,
			attr_on & A_REVERSE,
			attr_on & A_BLINK,
			attr_on & A_DIM,
			attr_on & A_BOLD,
			attr_on & A_INVIS,
			attr_on & A_PROTECT,
			attr_on & A_ALTCHARSET),
		  1, outc);
	}
    }
    else {
	if ((attr_off & A_ALTCHARSET) && exit_alt_charset_mode != NULL) {
	    tputs(exit_alt_charset_mode, 1, outc);
	    attr_off &= ~A_ALTCHARSET;
	}
	if ((attr_off & A_UNDERLINE) && exit_underline_mode != NULL) {
	    tputs(exit_underline_mode, 1, outc);
	    attr_off &= ~A_UNDERLINE;
	}
	if ((attr_off & A_STANDOUT) && exit_standout_mode != NULL) {
	    tputs(exit_standout_mode, 1, outc);
	    attr_off &= ~A_STANDOUT;
	}
	if (attr_off && exit_attribute_mode != NULL) {
	    tputs(exit_attribute_mode, 1, outc);
	    attr_on |= (attr & (chtype)(~A_COLOR));
	}
	if ((attr_on & A_ALTCHARSET) && enter_alt_charset_mode != NULL)
	    tputs(enter_alt_charset_mode, 1, outc);
	if ((attr_on & A_BLINK) && enter_blink_mode != NULL)
	    tputs(enter_blink_mode, 1, outc);
	if ((attr_on & A_BOLD) && enter_bold_mode != NULL)
	    tputs(enter_bold_mode, 1, outc);
	if ((attr_on & A_DIM) && enter_dim_mode != NULL)
	    tputs(enter_dim_mode, 1, outc);
	if ((attr_on & A_REVERSE) && enter_reverse_mode != NULL)
	    tputs(enter_reverse_mode, 1, outc);
	if ((attr_on & A_STANDOUT) && enter_standout_mode != NULL)
	    tputs(enter_standout_mode, 1, outc);
	if ((attr_on & A_PROTECT) && enter_protected_mode != NULL)
	    tputs(enter_protected_mode, 1, outc);
	if ((attr_on & A_INVIS) && enter_secure_mode != NULL)
	    tputs(enter_secure_mode, 1, outc);
	if ((attr_on & A_UNDERLINE) && enter_underline_mode != NULL)
	    tputs(enter_underline_mode, 1, outc);
	if ((attr_on & A_HORIZONTAL) && enter_horizontal_hl_mode != NULL)
	    tputs(enter_horizontal_hl_mode, 1, outc);
	if ((attr_on & A_LEFT) && enter_left_hl_mode != NULL)
	    tputs(enter_left_hl_mode, 1, outc);
	if ((attr_on & A_LOW) && enter_low_hl_mode != NULL)
	    tputs(enter_low_hl_mode, 1, outc);
	if ((attr_on & A_RIGHT) && enter_right_hl_mode != NULL)
	    tputs(enter_right_hl_mode, 1, outc);
	if ((attr_on & A_TOP) && enter_top_hl_mode != NULL)
	    tputs(enter_top_hl_mode, 1, outc);
	if ((attr_on & A_VERTICAL) && enter_vertical_hl_mode != NULL)
	    tputs(enter_vertical_hl_mode, 1, outc);
    }
    old_attr = attr;
    return OK;
}
