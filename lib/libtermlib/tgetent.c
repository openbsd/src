/*	$OpenBSD: tgetent.c,v 1.3 1996/12/09 01:18:19 tholo Exp $	*/

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
static char rcsid[] = "$OpenBSD: tgetent.c,v 1.3 1996/12/09 01:18:19 tholo Exp $";
#endif

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include "term.h"
#include "term.private.h"

extern char *_ti_buf;

int
tgetent(bp, name)
     char *bp;
     const char *name;
{
    char *n;

    _ti_buf = bp;
    if (cur_term != NULL) {
	for (n = strtok(cur_term->names, "|"); n != NULL; n = strtok(NULL, "|"))
	    if (strcmp(name, n) == 0)
		return 1;
	del_curterm(cur_term);
    }
    if ((cur_term = calloc(sizeof(TERMINAL), 1)) == NULL)
	errx(1, "No memory for terminal description");
    if (isatty(STDOUT_FILENO))
	cur_term->fd = STDOUT_FILENO;
    else
	cur_term->fd = STDERR_FILENO;
    (void)_ti_tty_init();
    return _ti_getterm(name);
}
