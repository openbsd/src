/*	$OpenBSD: lock.c,v 1.19 2002/08/15 21:49:40 deraadt Exp $	*/
/*	$NetBSD: lock.c,v 1.8 1996/05/07 18:32:31 jtc Exp $	*/

/*
 * Copyright (c) 1980, 1987, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Bob Toxen.
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
"@(#) Copyright (c) 1980, 1987, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)lock.c	8.1 (Berkeley) 6/6/93";
#endif
static char rcsid[] = "$OpenBSD: lock.c,v 1.19 2002/08/15 21:49:40 deraadt Exp $";
#endif /* not lint */

/*
 * Lock a terminal up until the given key is entered, until the root
 * password is entered, or the given interval times out.
 *
 * Timeout interval is by default TIMEOUT, it can be changed with
 * an argument of the form -time where time is in minutes
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <signal.h>

#include <ctype.h>
#include <err.h>
#include <pwd.h>
#include <readpassphrase.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <login_cap.h>
#include <bsd_auth.h>

#define	TIMEOUT	15

void bye(int);
void hi(int);

struct timeval	timeout;
struct timeval	zerotime;
time_t	nexttime;			/* keep the timeout time */
int	no_timeout;			/* lock terminal forever */

extern	char *__progname;

/*ARGSUSED*/
int
main(int argc, char *argv[])
{
	char hostname[MAXHOSTNAMELEN], s[BUFSIZ], s1[BUFSIZ], date[256];
	char *p, *style, *nstyle, *ttynam;
	struct itimerval ntimer, otimer;
	int ch, sectimeout, usemine;
	struct passwd *pw;
	struct tm *timp;
	time_t curtime;
	login_cap_t *lc;

	sectimeout = TIMEOUT;
	style = NULL;
	usemine = 0;
	no_timeout = 0;

	if (!(pw = getpwuid(getuid())))
		errx(1, "unknown uid %u.", getuid());

	lc = login_getclass(pw->pw_class);
	
	while ((ch = getopt(argc, argv, "a:npt:")) != -1)
		switch (ch) {
		case 'a':
			if (lc) {
				style = login_getstyle(lc, optarg, "auth-lock");
				if (style == NULL)
					errx(1,
					    "invalid authentication style: %s",
					    optarg);
			}
			usemine = 1;
			break;
		case 't':
			if ((sectimeout = atoi(optarg)) <= 0)
				errx(1, "illegal timeout value: %s", optarg);
			break;
		case 'p':
			usemine = 1;
			break;
		case 'n':
			no_timeout = 1;
			break;
		case '?':
		default:
			(void)fprintf(stderr,
			    "usage: %s [-n] [-p] [-a style] [-t timeout]\n",
			    __progname);
			exit(1);
	}
	timeout.tv_sec = sectimeout * 60;

	gethostname(hostname, sizeof(hostname));
	if (!(ttynam = ttyname(STDIN_FILENO)))
		errx(1, "not a terminal?");
	curtime = time(NULL);
	nexttime = curtime + (sectimeout * 60);
	timp = localtime(&curtime);
	strftime(date, sizeof(date), "%c", timp);

	if (!usemine) {
		/* get key and check again */
		if (!readpassphrase("Key: ", s, sizeof(s), RPP_ECHO_OFF) ||
		    *s == '\0')
			exit(0);
		/*
		 * Don't need EOF test here, if we get EOF, then s1 != s
		 * and the right things will happen.
		 */
		(void)readpassphrase("Again: ", s1, sizeof(s1), RPP_ECHO_OFF);
		if (strcmp(s1, s)) {
			warnx("\apasswords didn't match.");
			exit(1);
		}
		s[0] = '\0';
	}

	/* set signal handlers */
	(void)signal(SIGINT, hi);
	(void)signal(SIGQUIT, hi);
	(void)signal(SIGTSTP, hi);
	(void)signal(SIGALRM, bye);

	ntimer.it_interval = zerotime;
	ntimer.it_value = timeout;
	if (!no_timeout)
		setitimer(ITIMER_REAL, &ntimer, &otimer);

	/* header info */
	if (no_timeout) {
		(void)fprintf(stderr,
		    "%s: %s on %s. no timeout\ntime now is %s\n",
		    __progname, ttynam, hostname, date);
	} else {
		(void)fprintf(stderr,
		    "%s: %s on %s. timeout in %d minutes\ntime now is %s\n",
		    __progname, ttynam, hostname, sectimeout, date);
	}

	for (;;) {
		if (!readpassphrase("Key: ", s, sizeof(s), RPP_ECHO_OFF) ||
		    *s == '\0') {
			hi(0);
			continue;
		}
		if (usemine) {
			/*
			 * If user entered 's/key' or the style specified via
			 * the '-a' argument, auth_userokay() will prompt
			 * for a new password.  Otherwise, use what we have.
			 */
			if ((strcmp(s, "s/key") == 0 &&
			    (nstyle = login_getstyle(lc, "skey", "auth-lock")))
			    || ((nstyle = style) && strcmp(s, nstyle) == 0))
				p = NULL;
			else
				p = s;
			if (auth_userokay(pw->pw_name, nstyle, "auth-lock", p))
				break;
		} else if (strcmp(s, s1) == 0)
			break;
		(void)putc('\a', stderr);
	}

	exit(0);
}

void
hi(int dummy)
{
	char buf[1024], buf2[1024];
	time_t now;

	if (no_timeout)
		buf2[0] = '\0';
	else {
		now = time(NULL);
		(void)snprintf(buf2, sizeof buf2, " timeout in %d:%d minutes",
		    (nexttime - now) / 60, (nexttime - now) % 60);
	}
	snprintf(buf, sizeof buf, "%s: type in the unlock key.%s\n",
	    __progname, buf2);
	(void) write(STDERR_FILENO, buf, strlen(buf));
}

void
bye(int dummy)
{

	if (!no_timeout)
		warnx("timeout");
	_exit(1);
}
