/*	$OpenBSD: restartterm.c,v 1.1.1.1 1996/05/31 05:40:02 tholo Exp $	*/

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
static char rcsid[] = "$OpenBSD: restartterm.c,v 1.1.1.1 1996/05/31 05:40:02 tholo Exp $";
#endif

#include "term.h"

/*
 * Reinitialize terminal setup after a restart, eg. with the use
 * of undump
 *
 * The terminal and speed may have changed, but it is assumed that
 * the size of the terminal remain constant
 */
int
restartterm(name, fd, errret)
     const char *name;
     int fd;
     int *errret;
{
    struct termios pmode, smode;
    int r, l, c;

    pmode = cur_term->pmode;
    smode = cur_term->smode;
    l = lines;
    c = columns;
    r = setupterm(name, fd, errret);
    cfsetspeed(&pmode, cur_term->baudrate);
    cfsetspeed(&smode, cur_term->baudrate);
    cur_term->pmode = pmode;
    cur_term->smode = smode;
    lines = l;
    columns = c;
    return r;
}
