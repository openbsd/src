/*	$OpenBSD: fillcap.c,v 1.4 1998/01/17 16:35:05 millert Exp $	*/

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
static char rcsid[] = "$OpenBSD: fillcap.c,v 1.4 1998/01/17 16:35:05 millert Exp $";
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "term.h"
#include "term.private.h"

void
_ti_fillcap(term)
	struct _terminal *term;
{
    TERMINAL *save = cur_term;
    char buf[4096];

    cur_term = term;
    if (carriage_return == NULL)
	if (carriage_return_delay > 0) {
	    sprintf(buf, "\r$<%d>", carriage_return_delay);
	    carriage_return = strdup(buf);
	}
	else
	    carriage_return = strdup("\r");
    if (cursor_left == NULL)
	if (backspace_delay > 0) {
	    sprintf(buf, "\b$<%d>", backspace_delay);
	    cursor_left = strdup(buf);
	}
	else if (backspaces_with_bs)
	    cursor_left = strdup("\b");
	else if (backspace_if_not_bs != NULL)
	    cursor_left = strdup(backspace_if_not_bs);
    if (cursor_down == NULL)
	if (linefeed_if_not_lf != NULL)
	    cursor_down = strdup(linefeed_if_not_lf);
	else if (linefeed_is_newline != 1)
	    if (new_line_delay > 0) {
		sprintf(buf, "\n$<%d>", new_line_delay);
		cursor_down = strdup(buf);
	    }
	    else
		cursor_down = strdup("\n");
    if (newline == NULL)
	if (linefeed_is_newline == 1) {
	    if (new_line_delay > 0) {
		sprintf(buf, "\n$<%d>", new_line_delay);
		newline = strdup(buf);
	    }
	    else
		newline = strdup("\n");
	}
	else if (carriage_return != NULL && carriage_return_delay <= 0) {
	    if (linefeed_if_not_lf != NULL) {
		strncpy(buf, carriage_return, (sizeof(buf) >> 1) -1);
		buf[(sizeof(buf) >> 1) -1] = '\0';
		strncat(buf, linefeed_if_not_lf, sizeof(buf) - strlen(buf));
	    }
	    else if (new_line_delay > 0)
		sprintf(buf, "%s\n$<%d>", carriage_return, new_line_delay);
	    else {
		strncpy(buf, carriage_return, sizeof(buf) >> 1);
		buf[(sizeof(buf) >> 1) - 1] = '\0';
		strncat(buf, "\n", sizeof(buf) - strlen(buf));
	    }
	    newline = strdup(buf);
	}
    if (return_does_clr_eol || no_correctly_working_cr) {
	if (carriage_return != NULL)
	    free(carriage_return);
	carriage_return = NULL;
    }
    if (tab == NULL)
	if (horizontal_tab_delay > 0) {
	    sprintf(buf, "\t$<%d>", horizontal_tab_delay);
	    tab = strdup(buf);
	}
	else
	    tab = strdup("\t");
    if (init_tabs == 0 && has_hardware_tabs == 1)
	init_tabs = 8;
    if (key_backspace == NULL)
	key_backspace = strdup("\b");
    if (key_left == NULL)
	key_left = strdup("\b");
    if (key_down == NULL)
	key_down = strdup("\n");
    if (gnu_has_meta_key == 1 && has_meta_key == 0)
	has_meta_key = 1;
    cur_term = save;
}
