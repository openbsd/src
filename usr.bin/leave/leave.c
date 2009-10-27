/*	$OpenBSD: leave.c,v 1.12 2009/10/27 23:59:39 deraadt Exp $	*/
/*	$NetBSD: leave.c,v 1.4 1995/07/03 16:50:13 phil Exp $	*/

/*
 * Copyright (c) 1980, 1988, 1993
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

#include <sys/param.h>
#include <sys/time.h>
#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static __dead void	usage(void);
static void		doalarm(u_int secs);

#define SECOND  1
#define MINUTE  (SECOND * 60)
#define	FIVEMIN	(5 * MINUTE)
#define HOUR    (MINUTE * 60)

/*
 * leave [[+]hhmm]
 *
 * Reminds you when you have to leave.
 * Leave prompts for input and goes away if you hit return.
 * It nags you like a mother hen.
 */
int
main(int argc, char *argv[])
{
	u_int secs;
	int hours, minutes;
	char c, *cp;
	struct tm *t;
	time_t now;
	int plusnow = 0, twentyfour;
	char buf[50];
	
	if (setlinebuf(stdout) != 0)
		errx(1, "Cannot set stdout to line buffered.");

	if (argc < 2) {
		(void)fputs("When do you have to leave? ", stdout);
		cp = fgets(buf, sizeof(buf), stdin);
		if (cp == NULL || *cp == '\n')
			exit(0);
	} else if (argc > 2)
		usage();
	else
		cp = argv[1];

	if (*cp == '+') {
		plusnow = 1;
		++cp;
	}

	for (hours = 0; (c = *cp) && c != '\n'; ++cp) {
		if (!isdigit(c))
			usage();
		hours = hours * 10 + (c - '0');
	}
	minutes = hours % 100;
	hours /= 100;
	/* determine 24 hours mode */
	twentyfour = hours > 12;

	if (minutes < 0 || minutes > 59)
		usage();
	if (plusnow)
		secs = (hours * HOUR) + (minutes * MINUTE);
	else {
		if (hours > 23)
			usage();
		(void)time(&now);
		t = localtime(&now);
		while (t->tm_hour > hours || 
		    (t->tm_hour == hours && t->tm_min >= minutes)) {
			if (twentyfour)
				hours += 24;
			else
				hours += 12;
		}

		secs = (hours - t->tm_hour) * HOUR;
		secs += (minutes - t->tm_min) * MINUTE;
	}
	doalarm(secs);
	exit(0);
}

static void
doalarm(u_int secs)
{
	int bother;
	time_t daytime;
	pid_t pid;

	switch (pid = fork()) {
	case 0:
		break;
	case -1:
		err(1, "Fork failed");
		/* NOTREACHED */
	default:
		(void)time(&daytime);
		daytime += secs;
		printf("Alarm set for %.16s. (pid %ld)\n",
		    ctime(&daytime), (long)pid);
		exit(0);
	}
	sleep(2);			/* let parent print set message */

	/*
	 * if write fails, we've lost the terminal through someone else
	 * causing a vhangup by logging in.
	 */
	if (secs >= FIVEMIN) {
		sleep(secs - FIVEMIN);
		if (puts("\a\aYou have to leave in 5 minutes.") == EOF)
			exit(0);
		secs = FIVEMIN;
	}

	if (secs >= MINUTE) {
		sleep(secs - MINUTE);
		if (puts("\a\aJust one more minute!") == EOF)
			exit(0);
	}

	for (bother = 10; bother--;) {
		sleep(MINUTE);
		if (puts("\a\aTime to leave!") == EOF)
			exit(0);
	}

	puts("\a\aThat was the last time I'll tell you.  Bye.");
	exit(0);
}

static __dead void
usage(void)
{
	fprintf(stderr, "usage: leave [[+]hhmm]\n");
	exit(1);
}
