/*	$OpenBSD: lock.c,v 1.3 1996/06/26 05:35:56 deraadt Exp $	*/
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
static char rcsid[] = "$OpenBSD: lock.c,v 1.3 1996/06/26 05:35:56 deraadt Exp $";
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define	TIMEOUT	15

void quit(), bye(), hi();

struct timeval	timeout;
struct timeval	zerotime;
struct termios	tty, ntty;
long	nexttime;			/* keep the timeout time */

/*ARGSUSED*/
main(argc, argv)
	int argc;
	char **argv;
{
	extern char *optarg;
	struct passwd *pw;
	struct timeval timval;
	struct itimerval ntimer, otimer;
	struct tm *timp;
	time_t curtime;
	int ch, sectimeout, usemine;
	char *ap, *mypw, *ttynam, *tzn;
	char hostname[MAXHOSTNAMELEN], s[BUFSIZ], s1[BUFSIZ];
	char *crypt();

	sectimeout = TIMEOUT;
	mypw = NULL;
	usemine = 0;

	if (!(pw = getpwuid(getuid())))
		errx(1, "unknown uid %d.", getuid());
	
	while ((ch = getopt(argc, argv, "pt:")) != EOF)
		switch((char)ch) {
		case 't':
			if ((sectimeout = atoi(optarg)) <= 0)
				errx(1, "illegal timeout value: %s", optarg);
			break;
		case 'p':
			usemine = 1;
			mypw = strdup(pw->pw_passwd);
			break;
		case '?':
		default:
			(void)fprintf(stderr,
			    "usage: lock [-p] [-t timeout]\n");
			exit(1);
	}
	timeout.tv_sec = sectimeout * 60;

	setuid(getuid());		/* discard privs */

	if (tcgetattr(0, &tty) < 0)	/* get information for header */
		exit(1);
	gethostname(hostname, sizeof(hostname));
	if (!(ttynam = ttyname(0)))
		errx(1, "not a terminal?");
	if (gettimeofday(&timval, (struct timezone *)NULL))
		err(1, "gettimeofday");
	curtime = timval.tv_sec;
	nexttime = timval.tv_sec + (sectimeout * 60);
	timp = localtime(&curtime);
	ap = asctime(timp);
	tzn = timp->tm_zone;

	(void)signal(SIGINT, quit);
	(void)signal(SIGQUIT, quit);
	ntty = tty; ntty.c_lflag &= ~ECHO;
	(void)tcsetattr(0, TCSADRAIN, &ntty);

	if (!mypw) {
		/* get key and check again */
		(void)printf("Key: ");
		if (!fgets(s, sizeof(s), stdin) || *s == '\n')
			quit();
		(void)printf("\nAgain: ");
		/*
		 * Don't need EOF test here, if we get EOF, then s1 != s
		 * and the right things will happen.
		 */
		(void)fgets(s1, sizeof(s1), stdin);
		(void)putchar('\n');
		if (strcmp(s1, s)) {
			(void)printf("\alock: passwords didn't match.\n");
			(void)tcsetattr(0, TCSADRAIN, &tty);
			exit(1);
		}
		s[0] = NULL;
		mypw = s1;
	}

	/* set signal handlers */
	(void)signal(SIGINT, hi);
	(void)signal(SIGQUIT, hi);
	(void)signal(SIGTSTP, hi);
	(void)signal(SIGALRM, bye);

	ntimer.it_interval = zerotime;
	ntimer.it_value = timeout;
	setitimer(ITIMER_REAL, &ntimer, &otimer);

	/* header info */
(void)printf("lock: %s on %s. timeout in %d minutes\ntime now is %.20s%s%s",
	    ttynam, hostname, sectimeout, ap, tzn, ap + 19);

	for (;;) {
		(void)printf("Key: ");
		if (!fgets(s, sizeof(s), stdin)) {
			clearerr(stdin);
			hi();
			continue;
		}
		if (usemine) {
			s[strlen(s) - 1] = '\0';
#ifdef SKEY
			if (strcasecmp(s, "s/key") == 0) {
				if (skey_auth(pw->pw_name))
					break;
			}
#endif
			if (!strcmp(mypw, crypt(s, mypw)))
				break;
		}
		else if (!strcmp(s, s1))
			break;
		(void)printf("\a\n");
		if (tcsetattr(0, TCSADRAIN, &ntty) < 0)
			exit(1);
	}
	quit();
}

#ifdef SKEY
/*
 * We can't use libskey's skey_authenticate() since it
 * handles signals in a way that's inappropriate
 * for our needs. Instead we roll our own.
 */
int
skey_auth(char *user)
{
	char s[128], *ask, *skey_keyinfo __P((char *name));
	int ret = 0;

	if (!skey_haskey(user) && (ask = skey_keyinfo(user))) {
		printf("\n[%s]\nResponse: ", ask);		
		if (!fgets(s, sizeof(s), stdin) || *s == '\n')
			clearerr(stdin);
		else {
			s[strlen(s) - 1] = '\0';
			if (skey_passcheck(user, s) != -1)
				ret = 1;
		}
	} else
		printf("Sorry, you have no s/key.\n");
	return ret;
}
#endif

void
hi()
{
	struct timeval timval;

	if (!gettimeofday(&timval, (struct timezone *)NULL))
(void)printf("lock: type in the unlock key. timeout in %ld:%ld minutes\n",
	    (nexttime - timval.tv_sec) / 60, (nexttime - timval.tv_sec) % 60);
}

void
quit()
{
	(void)putchar('\n');
	(void)tcsetattr(0, TCSADRAIN, &tty);
	exit(0);
}

void
bye()
{
	(void)tcsetattr(0, TCSADRAIN, &tty);
	(void)printf("lock: timeout\n");
	exit(1);
}
