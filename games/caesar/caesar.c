/*	$OpenBSD: caesar.c,v 1.6 1998/03/12 09:06:43 pjanzen Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Adams.
 *
 * Authors:
 *	Stan King, John Eldridge, based on algorithm suggested by
 *	Bob Morris
 * 29-Sep-82
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
static char copyright[] =
"@(#) Copyright (c) 1989, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)caesar.c	8.1 (Berkeley) 5/31/93";
#else
static char rcsid[] = "$OpenBSD: caesar.c,v 1.6 1998/03/12 09:06:43 pjanzen Exp $";
#endif
#endif /* not lint */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>

#define	LINELENGTH	2048
#define	ROTATE(ch, perm) \
	isupper(ch) ? ('A' + (ch - 'A' + perm) % 26) : \
	    islower(ch) ? ('a' + (ch - 'a' + perm) % 26) : ch

/*
 * letter frequencies (taken from some unix(tm) documentation)
 * (unix is a trademark of Bell Laboratories)
 */
double stdf[26] = {
	7.97, 1.35, 3.61, 4.78, 12.37, 2.01, 1.46, 4.49, 6.39, 0.04,
	0.42, 3.81, 2.69, 5.92,  6.96, 2.91, 0.08, 6.63, 8.77, 9.68,
	2.62, 0.81, 1.88, 0.23,  2.07, 0.06,
};

void printit __P((int));
void usage   __P(());


int
main(argc, argv)
	int argc;
	char **argv;
{
	register int ch, dot, i, nread, winnerdot = 0;
	register char *inbuf, *p, **av;
	int obs[26], try, winner;

	/* revoke privs */
	setegid(getgid());
	setgid(getgid());

	if (argc > 1) {
		if ((i = atoi(argv[1])))
			printit(i);
		else
			usage();
	}

	/* check to see if we were called as rot13 */
	av = argv;
	p = strrchr(*av, '/');
	if (p++ == NULL)
		p = *av;
	if (strcmp(p,"rot13") == 0)
		printit(13);

	if (!(inbuf = malloc(LINELENGTH)))
		errx(1, "out of memory.");

	/* adjust frequency table to weight low probs REAL low */
	for (i = 0; i < 26; ++i)
		stdf[i] = log(stdf[i]) + log(26.0 / 100.0);

	/* zero out observation table */
	memset(obs, 0, 26 * sizeof(int));

	nread = 0;
	while ((nread < LINELENGTH) && ((ch = getchar()) != EOF)) {
		inbuf[nread++] = ch;
		if (islower(ch))
			++obs[ch - 'a'];
		else if (isupper(ch))
			++obs[ch - 'A'];
	}

	/*
	 * now "dot" the freqs with the observed letter freqs
	 * and keep track of best fit
	 */
	for (try = winner = 0; try < 26; ++try) { /* += 13) { */
		dot = 0;
		for (i = 0; i < 26; i++)
			dot += obs[i] * stdf[(i + try) % 26];
		if (dot > winnerdot) {
			/* got a new winner! */
			winner = try;
			winnerdot = dot;
		}
	}

	/* dump the buffer before calling printit */
	for (i = 0; i < nread; ++i) {
		ch = inbuf[i];
		putchar(ROTATE(ch, winner));
	}
	printit(winner);
	/* NOT REACHED */
}

void
printit(int rot)
{
	register int ch;

	if ((rot < 0) || ( rot >= 26))
		errx(1, "bad rotation value");
	
	while ((ch = getchar()) != EOF)
		putchar(ROTATE(ch, rot));
	exit(0);
}

void
usage()
{
	fprintf(stderr,"usage: caesar [rotation]\n");
	exit(1);
}
