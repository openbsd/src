/*	$OpenBSD: sleep.c,v 1.26 2018/02/04 02:18:15 cheloha Exp $	*/
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

#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <err.h>

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

	if (pledge("stdio", NULL) == -1)
		err(1, "pledge");

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
		if (!isdigit((unsigned char)*cp))
			errx(1, "seconds is invalid: %s", *argv);
		t = (secs * 10) + (*cp++ - '0');
		if (t / 10 != secs)	/* oflow */
			errx(1, "seconds is too large: %s", *argv);
		secs = t;
	}

	/* Handle fractions of a second */
	if (*cp == '.') {
		cp++;
		for (i = 100000000; i > 0; i /= 10) {
			if (*cp == '\0')
				break;
			if (!isdigit((unsigned char)*cp))
				errx(1, "seconds is invalid: %s", *argv);
			nsecs += (*cp++ - '0') * i;
		}

		/*
		 * We parse all the way down to nanoseconds
		 * in the above for loop. Be pedantic about
		 * checking the rest of the argument.
		 */
		while (*cp != '\0') {
			if (!isdigit((unsigned char)*cp++))
				errx(1, "seconds is invalid: %s", *argv);
		}
	}

	while (secs > 0 || nsecs > 0) {
		/*
		 * nanosleep(2) supports a maximum of 100 million
		 * seconds, so we break the nap up into multiple
		 * calls if we have more than that.
		 */
		if (secs > 100000000) {
			rqtp.tv_sec = 100000000;
			rqtp.tv_nsec = 0;
		} else {
			rqtp.tv_sec = secs;
			rqtp.tv_nsec = nsecs;
		}
		if (nanosleep(&rqtp, NULL))
			err(1, NULL);
		secs -= rqtp.tv_sec;
		nsecs -= rqtp.tv_nsec;
	}
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
