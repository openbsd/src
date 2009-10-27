/*	$OpenBSD: rsh.c,v 1.40 2009/10/27 23:59:43 deraadt Exp $	*/

/*-
 * Copyright (c) 1983, 1990 The Regents of the University of California.
 * All rights reserved.
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
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/file.h>

#include <netinet/in.h>

#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pathnames.h"

__dead void usage(void);
void sendsig(int);
char *copyargs(char **argv);
void talk(int, sigset_t *, int, int);

/*
 * rsh - remote shell
 */
int rfd2;

int
main(int argc, char *argv[])
{
	struct passwd *pw;
	struct servent *sp;
	sigset_t mask, omask;
	int argoff = 0, asrsh = 0, ch, dflag = 0, nflag = 0, one = 1, rem;
	char *args, *host = NULL, *user = NULL;
	pid_t pid = 0;
	extern char *__progname;
	uid_t uid;

	/* if called as something other than "rsh", use it as the host name */
	if (strcmp(__progname, "rsh") != 0)
		host = __progname;
	else
		asrsh = 1;

	/* handle "rsh host flags" */
	if (!host && argc > 2 && argv[1][0] != '-') {
		host = argv[1];
		argoff = 1;
	}

	while ((ch = getopt(argc - argoff, argv + argoff, "8KLdel:nw")) != -1)
		switch(ch) {
		case '8':	/* -8KLew are ignored to allow rlogin aliases */
		case 'K':
		case 'L':
		case 'e':
		case 'w':
			break;
		case 'd':
			dflag = 1;
			break;
		case 'l':
			user = optarg;
			break;
		case 'n':
			nflag = 1;
			break;
		default:
			usage();
		}
	optind += argoff;

	uid = getuid();

	/* if haven't gotten a host yet, do so */
	if (!host && !(host = argv[optind++]))
		usage();

	/* if no command, login to remote host via ssh. */
	if (!argv[optind]) {
		if (setresuid(uid, uid, uid) == -1)
			err(1, "setresuid");
		if (asrsh)
			*argv = "ssh";
		execv(_PATH_SSH, argv);
		errx(1, "can't exec %s", _PATH_SSH);
	}

	argc -= optind;
	argv += optind;

	if (geteuid() != 0)
		errx(1, "must be setuid root");
	if ((pw = getpwuid(uid)) == NULL)
		errx(1, "unknown user ID %u", uid);
	if (user == NULL)
		user = pw->pw_name;

	args = copyargs(argv);

	if ((sp = getservbyname("shell", "tcp")) == NULL)
		errx(1, "shell/tcp: unknown service");

	(void)unsetenv("RSH");		/* no tricks with rcmd(3) */

	rem = rcmd_af(&host, sp->s_port, pw->pw_name, user, args, &rfd2,
	    PF_UNSPEC);
	if (rem < 0)
		exit(1);
	if (rfd2 < 0)
		errx(1, "can't establish stderr");

	if (setresuid(uid, uid, uid) == -1)
		err(1, "setresuid");

	if (dflag) {
		if (setsockopt(rem, SOL_SOCKET, SO_DEBUG, &one,
		    sizeof(one)) < 0)
			warn("setsockopt");
		if (setsockopt(rfd2, SOL_SOCKET, SO_DEBUG, &one,
		    sizeof(one)) < 0)
			warn("setsockopt");
	}
	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGQUIT);
	sigaddset(&mask, SIGTERM);
	sigprocmask(SIG_BLOCK, &mask, &omask);
	if (signal(SIGINT, SIG_IGN) != SIG_IGN)
		(void)signal(SIGINT, sendsig);
	if (signal(SIGQUIT, SIG_IGN) != SIG_IGN)
		(void)signal(SIGQUIT, sendsig);
	if (signal(SIGTERM, SIG_IGN) != SIG_IGN)
		(void)signal(SIGTERM, sendsig);

	if (!nflag) {
		if ((pid = fork()) < 0)
			err(1, "fork");
	}

	(void)ioctl(rfd2, FIONBIO, &one);
	(void)ioctl(rem, FIONBIO, &one);

	talk(nflag, &omask, pid, rem);

	if (!nflag)
		(void)kill(pid, SIGKILL);

	return 0;
}

void
talk(int nflag, sigset_t *omask, pid_t pid, int rem)
{
	int cc, wc;
	char *bp;
	struct pollfd pfd[2];
	char buf[BUFSIZ];

	if (!nflag && pid == 0) {
		(void)close(rfd2);

reread:		errno = 0;
		if ((cc = read(STDIN_FILENO, buf, sizeof buf)) <= 0)
			goto done;
		bp = buf;

		pfd[0].fd = rem;
		pfd[0].events = POLLOUT;
rewrite:
		if (poll(pfd, 1, INFTIM) < 0) {
			if (errno != EINTR)
				err(1, "poll");
			goto rewrite;
		}
		if (pfd[0].revents & (POLLERR|POLLHUP|POLLNVAL))
			err(1, "poll");
		wc = write(rem, bp, cc);
		if (wc < 0) {
			if (errno == EWOULDBLOCK)
				goto rewrite;
			goto done;
		}
		bp += wc;
		cc -= wc;
		if (cc == 0)
			goto reread;
		goto rewrite;
done:
		(void)shutdown(rem, 1);
		exit(0);
	}

	sigprocmask(SIG_SETMASK, omask, NULL);
	pfd[1].fd = rfd2;
	pfd[1].events = POLLIN;
	pfd[0].fd = rem;
	pfd[0].events = POLLIN;
	do {
		if (poll(pfd, 2, INFTIM) < 0) {
			if (errno != EINTR)
				err(1, "poll");
			continue;
		}
		if ((pfd[0].revents & (POLLERR|POLLHUP|POLLNVAL)) ||
		    (pfd[1].revents & (POLLERR|POLLHUP|POLLNVAL)))
			err(1, "poll");
		if (pfd[1].revents & POLLIN) {
			errno = 0;
			cc = read(rfd2, buf, sizeof buf);
			if (cc <= 0) {
				if (errno != EWOULDBLOCK)
					pfd[1].revents = 0;
			} else
				(void)write(STDERR_FILENO, buf, cc);
		}
		if (pfd[0].revents & POLLIN) {
			errno = 0;
			cc = read(rem, buf, sizeof buf);
			if (cc <= 0) {
				if (errno != EWOULDBLOCK)
					pfd[0].revents = 0;
			} else
				(void)write(STDOUT_FILENO, buf, cc);
		}
	} while ((pfd[0].revents & POLLIN) || (pfd[1].revents & POLLIN));
}

void
sendsig(int signo)
{
	int save_errno = errno;

	(void)write(rfd2, &signo, 1);
	errno = save_errno;
}

char *
copyargs(char **argv)
{
	char **ap, *p, *args;
	size_t cc, len;

	cc = 0;
	for (ap = argv; *ap; ++ap)
		cc += strlen(*ap) + 1;
	if ((args = malloc(cc)) == NULL)
		err(1, NULL);
	for (p = args, ap = argv; *ap; ++ap) {
		len = strlcpy(p, *ap, cc);
		if (len >= cc)
			errx(1, "copyargs overflow");
		p += len;
		cc -= len;
		if (ap[1]) {
			*p++ = ' ';
			cc--;
		}
	}
	return(args);
}

__dead void
usage(void)
{
	(void)fprintf(stderr,
	    "usage: rsh [-dn] [-l username] hostname [command]\n");
	exit(1);
}
