/*	$NetBSD: cons.c,v 1.9 1995/10/04 06:54:42 thorpej Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 *
 * from: Utah Hdr: cons.c 1.7 92/02/28
 *
 *	@(#)cons.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/param.h>
#include <dev/cons.h>

#include <hp300/stand/consdefs.h>
#include <hp300/stand/samachdep.h>

struct consdev constab[] = {
#ifdef ITECONSOLE
	{ iteprobe,	iteinit,	itegetchar,	iteputchar },
#endif
#ifdef DCACONSOLE
	{ dcaprobe,	dcainit,	dcagetchar,	dcaputchar },
#endif
#ifdef DCMCONSOLE
	{ dcmprobe,	dcminit,	dcmgetchar,	dcmputchar },
#endif
	{ 0 },
};

int	curcons_scode;	/* select code of device currently being probed */
int	cons_scode;	/* final select code of console device */

struct consdev *cn_tab;
int noconsole;

void
cninit()
{
	register struct consdev *cp;

	cn_tab = NULL;
	noconsole = 1;
	cons_scode = 256;	/* larger than last valid select code */
	for (cp = constab; cp->cn_probe; cp++) {
		(*cp->cn_probe)(cp);
		if (cp->cn_pri > CN_DEAD &&
		    (cn_tab == NULL || cp->cn_pri > cn_tab->cn_pri)) {
			cn_tab = cp;
			cons_scode = curcons_scode;
		}
	}
	if (cn_tab) {
		(*cn_tab->cn_init)(cn_tab);
		noconsole = 0;
#if 0
		printf("console: ");
		if (cons_scode == -1)
			printf("internal grf\n");
		else
			printf("scode %d\n", cons_scode);
#endif
	}
}

int
cngetc()
{

	/* Note: the dev_t arguments are not used! */
	if (cn_tab)
		return((*cn_tab->cn_getc)(0));
	return(0);
}

int
cnputc(c)
	int c;
{

	/* Note: the dev_t arguments are not used! */
#ifdef ROMPRF
	extern int userom;

	if (userom)
		romputchar(c);
	else
#endif
	if (cn_tab)
		(*cn_tab->cn_putc)(0, c);

	return (0);
}
