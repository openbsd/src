/*	$OpenBSD: wizard.c,v 1.17 2014/12/31 15:45:57 tedu Exp $	*/
/*	$NetBSD: wizard.c,v 1.3 1995/04/24 12:21:41 cgd Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * The game adventure was originally written in Fortran by Will Crowther
 * and Don Woods.  It was later translated to C and enhanced by Jim
 * Gillogly.  This code is derived from software contributed to Berkeley
 * by Jim Gillogly at The Rand Corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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

/*	Re-coding of advent in C: privileged operations			*/

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "hdr.h"
#include "extern.h"

#if 0
void
datime(int *d, int *t)
{
	time_t  tvec;
	struct tm *tptr;

	time(&tvec);
	tptr = localtime(&tvec);
	/* day since 1977  (mod leap)   */
	*d = tptr->tm_yday + 365 * (tptr->tm_year - 77);
	/* bug: this will overflow in the year 2066 AD                  */
	/* it will be attributed to Wm the C's millenial celebration    */
	/* and minutes since midnite */
	*t = tptr->tm_hour * 60 + tptr->tm_min;
}				/* pretty painless              */
#endif

char    magic[6];

void
poof(void)
{
	strlcpy(magic, DECR(d,w,a,r,f), sizeof magic);
	latncy = 45;
}

int
Start(void)
{
	time_t  t, delay;

	time(&t);
	delay = (t - savet) / 60;	/* Minutes	*/
	saved = -1;

	if (delay >= latncy)
		return (FALSE);
	printf("This adventure was suspended a mere %d minute%s ago.",
		(int)delay, delay == 1 ? "" : "s");
	if (delay <= latncy / 3) {
		mspeak(2);
		exit(0);
	}
	mspeak(8);
	if (!wizard()) {
		mspeak(9);
		exit(0);
	}
	return (FALSE);
}

int
wizard(void)		/* not as complex as advent/10 (for now)	*/
{
	if (!yesm(16, 0, 7))
		return (FALSE);
	mspeak(17);
	getin(wd1, sizeof(wd1), wd2, sizeof(wd2));
	if (!weq(wd1, magic)) {
		mspeak(20);
		return (FALSE);
	}
	mspeak(19);
	return (TRUE);
}

void
ciao(void)
{
	int	ch;
	char   *c;
	char    fname[PATH_MAX];

	printf("What would you like to call the saved version?\n");
	for (c = fname; c - fname < sizeof(fname); c++) {
		if ((ch = getchar()) == '\n' || ch == EOF)
			break;
		*c = ch;
	}
	if (c - fname == sizeof(fname)) {
		c--;
		FLUSHLINE;
	}
	*c = '\0';
	if (save(fname) != 0)
		return;		/* Save failed */
	printf("To resume, say \"adventure %s\".\n", fname);
	printf("\"With these rooms I might now have been familiarly acquainted.\"\n");
	exit(0);
}


int
ran(int range)
{
	return (arc4random_uniform(range));
}
