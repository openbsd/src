/*	$OpenBSD: setupterm.c,v 1.2 1996/06/02 20:19:29 tholo Exp $	*/

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
static char rcsid[] = "$OpenBSD: setupterm.c,v 1.2 1996/06/02 20:19:29 tholo Exp $";
#endif

#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include "term.h"
#include "term.private.h"

extern char *_ti_buf;

/*
 * Called to set up a terminal description for the named
 * terminal.  If no terminal is named, attempt to use the
 * one named in the TERM environment variable
 *
 * If an error is encountered, return it in *errstat if
 * set.  If not set, abort with an error message
 */
int
setupterm(name, fd, errstat)
     const char *name;
     int fd;
     int *errstat;
{
    int ret;

    if (name == NULL)
	name = getenv("TERM");
    _ti_buf = NULL;
    if (cur_term != NULL)
	del_curterm(cur_term);
    if ((cur_term = calloc(sizeof(TERMINAL), 1)) == NULL)
	errx(1, "No memory for terminal description");
    if (fd == STDOUT_FILENO && !isatty(fd))
	fd = STDERR_FILENO;
    cur_term->fd = fd;
    ret = _ti_getterm(name);
    if (errstat == NULL && ret < 1) {
	if (ret < 0)
	    errx(1, "Terminal description database could not be found");
	else
	    errx(1, "Terminal '%s' not found", name);
    }
    else if (errstat != NULL)
	*errstat = ret;
    (void) _ti_tty_init();
    return ret;
}
