/*	$OpenBSD: tty_init.c,v 1.1.1.1 1996/05/31 05:40:02 tholo Exp $	*/

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
static char rcsid[] = "$OpenBSD: tty_init.c,v 1.1.1.1 1996/05/31 05:40:02 tholo Exp $";
#endif

#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "term.h"
#include "term.private.h"

static unsigned int tspeed[] = {
    B0, B50, B75, B110, B134, B150, B200, B300, B600, B1200, B1800,
    B2400, B4800, B9600, B19200, B38400
};

#undef ospeed
short ospeed;

/*
 * Low lever terminal initialization; get current terminal parameters
 * and save current speed in cur_term and ospeed
 */
int
_ti_tty_init()
{
    struct termios ti;
    int speed, i;

    tcgetattr(cur_term->fd, &ti);
    cur_term->baudrate = cfgetospeed(&ti);
    speed = cur_term->baudrate;
    for (i = 0; i < sizeof(tspeed) / sizeof(tspeed[0]) && speed > tspeed[i] ; i++)
	;
    ospeed = i < sizeof(tspeed) / sizeof(tspeed[0]) ? i : 0;
    return OK;
}
