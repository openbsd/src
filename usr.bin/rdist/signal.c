/*	$OpenBSD: signal.c,v 1.4 1998/06/26 21:21:21 millert Exp $	*/

/*
 * Copyright (c) 1993 Regents of the University of California.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
#if 0
static char RCSid[] = 
"$From: signal.c,v 6.1 1993/07/15 22:47:30 mcooper Exp mcooper $";
#else
static char RCSid[] = 
"$OpenBSD: signal.c,v 1.4 1998/06/26 21:21:21 millert Exp $";
#endif

static char sccsid[] = "@(#)signal.c";

static char copyright[] =
"@(#) Copyright (c) 1993 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#include "defs.h"

#if     defined(NEED_SIGBLOCK)
static int current_mask = 0;

int sigblock(mask)
int mask;
{
    int sig;
    int m;
    int oldmask;

    oldmask = current_mask;
    for ( sig = 1, m = 1; sig <= MAXSIG; sig++, m <<= 1 ) {
        if (mask & m)  {
            sighold(sig);
            current_mask |= m;
        }
    }
    return oldmask;
}
#endif	/* NEED_SIGBLOCK */

#if	defined(NEED_SIGSETMASK)
int sigsetmask(mask)
int mask;
{
    int sig;
    int m;
    int oldmask;

    oldmask = current_mask;
    for ( sig = 1, m = 1; sig <= MAXSIG; sig++, m <<= 1 ) {
        if (mask & m)  {
            sighold(sig);
            current_mask |= m;
        }
        else  {
            sigrelse(sig);
            current_mask &= ~m;
        }
    }
    return oldmask;
}
#endif	/* NEED_SIGSETMASK */
