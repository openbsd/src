/*	$OpenBSD: del_curterm.c,v 1.2 1996/08/31 02:40:30 tholo Exp $	*/

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
static char rcsid[] = "$OpenBSD: del_curterm.c,v 1.2 1996/08/31 02:40:30 tholo Exp $";
#endif

#include <stdlib.h>
#include "term.h"

extern TERMINAL _ti_empty;

/*
 * Free storage associated with the terminal description
 * passed in.  Note that it is legal to free cur_term in
 * this manner.  If this is done, we also set cur_term
 * to NULL so that any references to that will cause an
 * error unless it is reinitialized with a call to
 * set_curterm() or setupterm()
 */
int
del_curterm(term)
    TERMINAL *term;
{
    int i;

    if (term == &_ti_empty)
	return OK;
    for (i = 0; i < _tStrCnt; i++)
	if (term->strs[i] != NULL)
	    free(term->strs[i]);
    free(term->name);
    free(term);
    /*
     * If the terminal description just freed was the current
     * one, set cur_term to NULL
     */
    if (term == cur_term)
	cur_term = &_ti_empty;
    return OK;
}
