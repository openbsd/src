/*	$OpenBSD: lock.c,v 1.46 2019/07/24 20:23:09 schwarze Exp $	*/
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

/*
 * Lock a terminal up until the given key or user password is entered,
 * or the given interval times out.
 */

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
#include <limits.h>

#include <login_cap.h>
#include <bsd_auth.h>

void bye(int);
void hi(int);
void usage(void);

int	no_timeout = 0;			/* lock terminal forever */

int
main(int argc, char *argv[])
{
	char hostname[HOST_NAME_MAX+1], s[BUFSIZ], s1[BUFSIZ], date[256];
	char hash[_PASSWORD_LEN];
	char *p, *style, *nstyle, *ttynam;
	struct itimerval ntimer, otimer;
	struct timeval timeout;
	int ch, sectimeout, usemine, cnt, tries = 10, backoff = 3;
	const char *errstr;
	struct passwd *pw;
	struct tm *timp;
	time_t curtime;
	login_cap_t *lc;

	sectimeout = 0;
	style = NULL;
	usemine = 0;
	memset(&timeout, 0, sizeof(timeout));

	if (pledge("stdio rpath wpath getpw tty proc exec", NULL) == -1)
		err(1, "pledge");

	if (!(pw = getpwuid(getuid())))
		errx(1, "unknown uid %u.", getuid());

	lc = login_getclass(pw->pw_class);
	if (lc != NULL) {
		/*
		 * We allow "login-tries" attempts to login but start
		 * slowing down after "login-backoff" attempts.
		 */
		tries = login_getcapnum(lc, "login-tries", 10, 10);
		backoff = login_getcapnum(lc, "login-backoff", 3, 3);
	}

	while ((ch = getopt(argc, argv, "a:npt:")) != -1) {
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
			sectimeout = strtonum(optarg, 1, INT_MAX, &errstr);
			if (errstr)
				errx(1, "timeout %s: %s", errstr, optarg);
			break;
		case 'p':
			usemine = 1;
			break;
		case 'n':
			no_timeout = 1;
			break;
		default:
			usage();
		}
	}
	if (sectimeout == 0)
		no_timeout = 1;

	gethostname(hostname, sizeof(hostname));
	if (usemine && lc == NULL)
		errx(1, "login class not found");
	if (!(ttynam = ttyname(STDIN_FILENO)))
		errx(1, "not a terminal?");
	curtime = time(NULL);
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
		readpassphrase("Again: ", s1, sizeof(s1), RPP_ECHO_OFF);
		if (strcmp(s1, s)) {
			warnx("\apasswords didn't match.");
			exit(1);
		}
		crypt_newhash(s, "bcrypt", hash, sizeof(hash));
		explicit_bzero(s, sizeof(s));
		explicit_bzero(s1, sizeof(s1));
	}

	/* set signal handlers */
	signal(SIGINT, hi);
	signal(SIGQUIT, hi);
	signal(SIGTSTP, hi);
	signal(SIGALRM, bye);

	if (!no_timeout) {
		timeout.tv_sec = (time_t)sectimeout * 60;
		memset(&ntimer, 0, sizeof(ntimer));
		ntimer.it_value = timeout;
		setitimer(ITIMER_REAL, &ntimer, &otimer);
	}

	/* header info */
	if (no_timeout) {
		fprintf(stderr,
		    "%s: %s on %s. no timeout\ntime now is %s\n",
		    getprogname(), ttynam, hostname, date);
	} else {
		fprintf(stderr,
		    "%s: %s on %s. timeout in %d minutes\ntime now is %s\n",
		    getprogname(), ttynam, hostname, sectimeout, date);
	}

	for (cnt = 0;;) {
		if (!readpassphrase("Key: ", s, sizeof(s), RPP_ECHO_OFF))
			continue;
		if (strlen(s) == 0) {
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
			if (auth_userokay(pw->pw_name, nstyle, "auth-lock",
			    p)) {
				explicit_bzero(s, sizeof(s));
				break;
			}
		} else if (crypt_checkpass(s, hash) == 0) {
			explicit_bzero(s, sizeof(s));
			explicit_bzero(hash, sizeof(hash));
			break;
		}
		putc('\a', stderr);
		cnt %= tries;
		if (++cnt > backoff) {
			sigset_t set, oset;
			sigfillset(&set);
			sigprocmask(SIG_BLOCK, &set, &oset);
			sleep((u_int)((cnt - backoff) * tries / 2));
			sigprocmask(SIG_SETMASK, &oset, NULL);
		}
	}

	exit(0);
}

void
hi(int signo)
{
	struct itimerval left;

	dprintf(STDERR_FILENO, "%s: type in the unlock key.",
	    getprogname());
	if (!no_timeout) {
		getitimer(ITIMER_REAL, &left);
		dprintf(STDERR_FILENO, " timeout in %lld:%02d minutes",
		    (long long)(left.it_value.tv_sec / 60),
		    (int)(left.it_value.tv_sec % 60));
	}
	dprintf(STDERR_FILENO, "\n");
}

void
bye(int signo)
{

	if (!no_timeout)
		warnx("timeout");
	_exit(1);
}

void
usage(void)
{
	fprintf(stderr, "usage: %s [-np] [-a style] [-t timeout]\n",
	    getprogname());
	exit(1);
}
