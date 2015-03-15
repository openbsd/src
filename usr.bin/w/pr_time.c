/*	$OpenBSD: pr_time.c,v 1.16 2015/03/15 00:41:28 millert Exp $	*/

/*-
 * Copyright (c) 1990, 1993, 1994
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

#include <sys/types.h>
#include <sys/time.h>

#include <stdio.h>
#include <string.h>

#include "extern.h"

/*
 * pr_attime --
 *	Print the time since the user logged in.
 */
void
pr_attime(time_t *started, time_t *now)
{
	static char buf[256];
	struct tm *tp;
	time_t diff;
	const char *fmt;
	int  today;

	today = localtime(now)->tm_yday;
	tp = localtime(started);
	diff = *now - *started;

	/* If more than a week, use day-month-year. */
	if (diff > SECSPERDAY * 7)
		fmt = "%d%b%y";

	/* If not today, use day-hour-am/pm. */
	else if (tp->tm_yday  != today ) {
		fmt = "%a%I%p";
	}

	/* Default is hh:mm{am,pm}. */
	else {
		fmt = "%l:%M%p";
	}

	(void)strftime(buf, sizeof buf -1, fmt, tp);
	buf[sizeof buf - 1] = '\0';
	(void)printf("%s", buf);
}

/*
 * pr_idle --
 *	Display the idle time.
 */
void
pr_idle(time_t idle)
{
	int days = idle / SECSPERDAY;

	/* If idle more than 36 hours, print as a number of days. */
	if (idle >= 36 * SECSPERHOUR) {
		if (days == 1)
			printf("  %dday ", days);
		else if (days < 10)
			printf(" %ddays ", days);
		else
			printf("%ddays ", days);
	}

	/* If idle more than an hour, print as HH:MM. */
	else if (idle >= SECSPERHOUR)
		(void)printf(" %2lld:%02lld ",
		    (long long)idle / SECSPERHOUR,
		    ((long long)idle % SECSPERHOUR) / 60);

	/* Else print the minutes idle. */
	else
		(void)printf("    %2lld ", (long long)idle / 60);
}
