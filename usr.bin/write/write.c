/*	$OpenBSD: write.c,v 1.26 2009/10/27 23:59:50 deraadt Exp $	*/
/*	$NetBSD: write.c,v 1.5 1995/08/31 21:48:32 jtc Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jef Poskanzer and Craig Leres of the Lawrence Berkeley Laboratory.
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
#include <sys/stat.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include <paths.h>
#include <pwd.h>
#include <unistd.h>
#include <utmp.h>
#include <err.h>
#include <vis.h>

void done(int sig);
void do_write(char *, char *, uid_t);
void wr_fputs(char *);
void search_utmp(char *, char *, int, char *, uid_t);
int term_chk(char *, int *, time_t *, int);
int utmp_chk(char *, char *);

int
main(int argc, char *argv[])
{
	char tty[MAXPATHLEN], *mytty, *cp;
	int msgsok, myttyfd;
	time_t atime;
	uid_t myuid;

	/* check that sender has write enabled */
	if (isatty(fileno(stdin)))
		myttyfd = fileno(stdin);
	else if (isatty(fileno(stdout)))
		myttyfd = fileno(stdout);
	else if (isatty(fileno(stderr)))
		myttyfd = fileno(stderr);
	else
		errx(1, "can't find your tty");
	if (!(mytty = ttyname(myttyfd)))
		errx(1, "can't find your tty's name");
	if ((cp = strrchr(mytty, '/')))
		mytty = cp + 1;
	if (term_chk(mytty, &msgsok, &atime, 1))
		exit(1);
	if (!msgsok)
		warnx("you have write permission turned off");

	myuid = getuid();

	/* check args */
	switch (argc) {
	case 2:
		search_utmp(argv[1], tty, sizeof tty, mytty, myuid);
		do_write(tty, mytty, myuid);
		break;
	case 3:
		if (!strncmp(argv[2], _PATH_DEV, sizeof(_PATH_DEV) - 1))
			argv[2] += sizeof(_PATH_DEV) - 1;
		if (utmp_chk(argv[1], argv[2]))
			errx(1, "%s is not logged in on %s",
			    argv[1], argv[2]);
		if (term_chk(argv[2], &msgsok, &atime, 1))
			exit(1);
		if (myuid && !msgsok)
			errx(1, "%s has messages disabled on %s",
			    argv[1], argv[2]);
		do_write(argv[2], mytty, myuid);
		break;
	default:
		(void)fprintf(stderr, "usage: write user [ttyname]\n");
		exit(1);
	}
	done(0);

	/* NOTREACHED */
	return (0);
}

/*
 * utmp_chk - checks that the given user is actually logged in on
 *     the given tty
 */
int
utmp_chk(char *user, char *tty)
{
	struct utmp u;
	int ufd;

	if ((ufd = open(_PATH_UTMP, O_RDONLY)) < 0)
		return(1);	/* no utmp, cannot talk to users */

	while (read(ufd, (char *) &u, sizeof(u)) == sizeof(u))
		if (strncmp(user, u.ut_name, sizeof(u.ut_name)) == 0 &&
		    strncmp(tty, u.ut_line, sizeof(u.ut_line)) == 0) {
			(void)close(ufd);
			return(0);
		}

	(void)close(ufd);
	return(1);
}

/*
 * search_utmp - search utmp for the "best" terminal to write to
 *
 * Ignores terminals with messages disabled, and of the rest, returns
 * the one with the most recent access time.  Returns as value the number
 * of the user's terminals with messages enabled, or -1 if the user is
 * not logged in at all.
 *
 * Special case for writing to yourself - ignore the terminal you're
 * writing from, unless that's the only terminal with messages enabled.
 */
void
search_utmp(char *user, char *tty, int ttyl, char *mytty, uid_t myuid)
{
	struct utmp u;
	time_t bestatime, atime;
	int ufd, nloggedttys, nttys, msgsok, user_is_me;
	char atty[UT_LINESIZE + 1];

	if ((ufd = open(_PATH_UTMP, O_RDONLY)) < 0)
		err(1, "%s", _PATH_UTMP);

	nloggedttys = nttys = 0;
	bestatime = 0;
	user_is_me = 0;
	while (read(ufd, (char *) &u, sizeof(u)) == sizeof(u))
		if (strncmp(user, u.ut_name, sizeof(u.ut_name)) == 0) {
			++nloggedttys;
			(void)strncpy(atty, u.ut_line, UT_LINESIZE);
			atty[UT_LINESIZE] = '\0';
			if (term_chk(atty, &msgsok, &atime, 0))
				continue;	/* bad term? skip */
			if (myuid && !msgsok)
				continue;	/* skip ttys with msgs off */
			if (strcmp(atty, mytty) == 0) {
				user_is_me = 1;
				continue;	/* don't write to yourself */
			}
			++nttys;
			if (atime > bestatime) {
				bestatime = atime;
				(void)strlcpy(tty, atty, ttyl);
			}
		}

	(void)close(ufd);
	if (nloggedttys == 0)
		errx(1, "%s is not logged in", user);
	if (nttys == 0) {
		if (user_is_me) {		/* ok, so write to yourself! */
			(void)strlcpy(tty, mytty, ttyl);
			return;
		}
		errx(1, "%s has messages disabled", user);
	} else if (nttys > 1)
		warnx("%s is logged in more than once; writing to %s",
		    user, tty);
}

/*
 * term_chk - check that a terminal exists, and get the message bit
 *     and the access time
 */
int
term_chk(char *tty, int *msgsokP, time_t *atimeP, int showerror)
{
	struct stat s;
	char path[MAXPATHLEN];

	(void)snprintf(path, sizeof(path), "%s%s", _PATH_DEV, tty);
	if (stat(path, &s) < 0) {
		if (showerror)
			warn("%s", path);
		return(1);
	}
	*msgsokP = (s.st_mode & S_IWGRP) != 0;	/* group write bit */
	*atimeP = s.st_atime;
	return(0);
}

/*
 * do_write - actually make the connection
 */
void
do_write(char *tty, char *mytty, uid_t myuid)
{
	char *login, *nows;
	struct passwd *pwd;
	time_t now;
	char path[MAXPATHLEN], host[MAXHOSTNAMELEN], line[512];
	gid_t gid;

	/* Determine our login name before the we reopen() stdout */
	if ((login = getlogin()) == NULL) {
		if ((pwd = getpwuid(myuid)))
			login = pwd->pw_name;
		else
			login = "???";
	}

	(void)snprintf(path, sizeof(path), "%s%s", _PATH_DEV, tty);
	if ((freopen(path, "w", stdout)) == NULL)
		err(1, "%s", path);

	/* revoke privs, now that we have opened the tty */
	gid = getgid();
	if (setresgid(gid, gid, gid) == -1)
		err(1, "setresgid");

	(void)signal(SIGINT, done);
	(void)signal(SIGHUP, done);

	/* print greeting */
	if (gethostname(host, sizeof(host)) < 0)
		(void)strlcpy(host, "???", sizeof host);
	now = time((time_t *)NULL);
	nows = ctime(&now);
	nows[16] = '\0';
	(void)printf("\r\n\007\007\007Message from %s@%s on %s at %s ...\r\n",
	    login, host, mytty, nows + 11);

	while (fgets(line, sizeof(line), stdin) != NULL)
		wr_fputs(line);
}

/*
 * done - cleanup and exit
 */
void
done(int sig)
{
	(void)write(STDOUT_FILENO, "EOF\r\n", 5);
	if (sig)
		_exit(0);
	else
		exit(0);
}

/*
 * wr_fputs - like fputs(), but makes control characters visible and
 *     turns \n into \r\n
 */
void
wr_fputs(char *s)
{
	u_char c;
	char visout[5], *s2;

#define	PUTC(c)	if (putchar(c) == EOF) goto err;

	for (; *s != '\0'; ++s) {
		c = toascii(*s);
		if (c == '\n') {
			PUTC('\r');
			PUTC('\n');
			continue;
		}
		vis(visout, c, VIS_SAFE|VIS_NOSLASH, s[1]);
		for (s2 = visout; *s2; s2++)
			PUTC(*s2);
	}
	return;

err:	err(1, NULL);
#undef PUTC
}
