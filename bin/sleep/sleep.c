/*	$OpenBSD: sleep.c,v 1.17 2004/09/16 08:35:49 deraadt Exp $	*/
/*	$NetBSD: sleep.c,v 1.8 1995/03/21 09:11:11 cgd Exp $	*/

/*
 * Copyright (c) 1988, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
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

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1988, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)sleep.c	8.3 (Berkeley) 4/2/94";
#else
static char rcsid[] = "$OpenBSD: sleep.c,v 1.17 2004/09/16 08:35:49 deraadt Exp $";
#endif
#endif /* not lint */

#include <ctype.h>
#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

extern char *__progname;

void usage(void);
void alarmh(int);

int
main(int argc, char *argv[])
{
	int ch;
	time_t secs = 0, t;
	char *cp;
	long nsecs = 0;
	struct timespec rqtp;
	int i;

	setlocale(LC_ALL, "");

	signal(SIGALRM, alarmh);

	while ((ch = getopt(argc, argv, "")) != -1)
		switch(ch) {
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();

	cp = *argv;
	while ((*cp != '\0') && (*cp != '.')) {
		if (!isdigit(*cp))
			usage();
		t = (secs * 10) + (*cp++ - '0');
		if (t / 10 != secs)	/* oflow */
			return (EINVAL);
		secs = t;
	}

	/* Handle fractions of a second */
	if (*cp == '.') {
		*cp++ = '\0';
		for (i = 100000000; i > 0; i /= 10) {
			if (*cp == '\0')
				break;
			if (!isdigit(*cp))
				usage();
			nsecs += (*cp++ - '0') * i;
		}

		/*
		 * We parse all the way down to nanoseconds
		 * in the above for loop. Be pedantic about
		 * checking the rest of the argument.
		 */
		while (*cp != '\0') {
			if (!isdigit(*cp++))
				usage();
		}
	}

	rqtp.tv_sec = secs;
	rqtp.tv_nsec = nsecs;

	if ((secs > 0) || (nsecs > 0))
		if (nanosleep(&rqtp, NULL))
			return (errno);
	return (0);
}

void
usage(void)
{
	(void)fprintf(stderr, "usage: %s seconds\n", __progname);
	exit(1);
}

/*
 * POSIX 1003.2 says sleep should exit with 0 return code on reception
 * of SIGALRM.
 */
/* ARGSUSED */
void
alarmh(int signo)
{
	/*
	 * exit() flushes stdio buffers, which is not legal in a signal
	 * handler. Use _exit().
	 */
	_exit(0);
}
