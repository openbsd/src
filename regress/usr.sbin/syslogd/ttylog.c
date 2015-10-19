/*
 * Copyright (c) 2015 Alexander Bluhm <bluhm@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/ioctl.h>
#include <sys/sockio.h>

#include <errno.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <util.h>
#include <utmp.h>

__dead void usage(void);
void timeout(int);
void terminate(int);
void iostdin(int);

FILE *lg;
char *tty;

__dead void
usage()
{
	fprintf(stderr, "usage: %s username logfile\n", getprogname());
	exit(2);
}

int
main(int argc, char *argv[])
{
	char buf[8192], ptyname[16], *username, *logfile;
	struct utmp utmp;
	int mfd, sfd;
	ssize_t n;
	int i;

	if (argc != 3)
		usage();
	username = argv[1];
	logfile = argv[2];

	if ((lg = fopen(logfile, "w")) == NULL)
		err(1, "fopen %s", logfile);
	if (setlinebuf(lg) != 0)
		err(1, "setlinebuf");

	if (signal(SIGTERM, terminate) == SIG_ERR)
		err(1, "signal SIGTERM");
	if (signal(SIGINT, terminate) == SIG_ERR)
		err(1, "signal SIGINT");

	if (openpty(&mfd, &sfd, ptyname, NULL, NULL) == -1)
		err(1, "openpty");
	fprintf(lg, "openpty %s\n", ptyname);
	if ((tty = strrchr(ptyname, '/')) == NULL)
		errx(1, "tty: %s", ptyname);
	tty++;

	/* login(3) searches for a controlling tty, use the created one */
	if (dup2(sfd, 1) == -1)
		err(1, "dup2 stdout");

	memset(&utmp, 0, sizeof(utmp));
	strlcpy(utmp.ut_line, tty, sizeof(utmp.ut_line));
	strlcpy(utmp.ut_name, username, sizeof(utmp.ut_name));
	time(&utmp.ut_time);
	login(&utmp);
	fprintf(lg, "login %s %s\n", username, tty);

	if (signal(SIGIO, iostdin) == SIG_ERR)
		err(1, "signal SIGIO");
	if (setpgid(0, 0) == -1)
		err(1, "setpgid");
	i = getpid();
	if (fcntl(0, F_SETOWN, i) == -1 &&
	    ioctl(0, SIOCSPGRP, &i) == -1)  /* pipe(2) with F_SETOWN broken */
		err(1, "fcntl F_SETOWN, ioctl SIOCSPGRP");
	if (fcntl(0, F_SETFL, O_ASYNC) == -1)
		err(1, "fcntl O_ASYNC");

	if (signal(SIGALRM, timeout) == SIG_ERR)
		err(1, "signal SIGALRM");
	if (alarm(30) == (unsigned int)-1)
		err(1, "alarm");

	fprintf(lg, "%s: started\n", getprogname());

	while ((n = read(mfd, buf, sizeof(buf))) > 0) {
		fprintf(lg, ">>> ");
		if (fwrite(buf, 1, n, lg) != (size_t)n)
			err(1, "fwrite %s", logfile);
		if (buf[n-1] != '\n')
			fprintf(lg, "\n");
	}
	if (n < 0)
		err(1, "read %s", ptyname);
	fprintf(lg, "EOF %s\n", ptyname);

	if (logout(tty) == 0)
		errx(1, "logout %s", tty);
	fprintf(lg, "logout %s\n", tty);

	errx(3, "EOF");
}

void
timeout(int sig)
{
	fprintf(lg, "signal timeout %d\n", sig);
	if (tty) {
		logout(tty);
		fprintf(lg, "logout %s\n", tty);
	}
	errx(3, "timeout");
}

void
terminate(int sig)
{
	fprintf(lg, "signal terminate %d\n", sig);
	if (tty) {
		logout(tty);
		fprintf(lg, "logout %s\n", tty);
	}
	errx(3, "terminate");
}

void
iostdin(int sig)
{
	char buf[8192];
	size_t n;

	fprintf(lg, "signal iostdin %d\n", sig);
	if ((n = read(0, buf, sizeof(buf))) < 0)
		err(1, "read stdin");
	if (tty) {
		logout(tty);
		fprintf(lg, "logout %s\n", tty);
	}
	if (n > 0)
		errx(3, "read stdin %zd bytes", n);
	exit(0);
}
