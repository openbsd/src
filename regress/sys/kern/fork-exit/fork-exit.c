/*	$OpenBSD: fork-exit.c,v 1.1 2021/04/28 17:59:53 bluhm Exp $	*/

/*
 * Copyright (c) 2021 Alexander Bluhm <bluhm@openbsd.org>
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

#include <sys/select.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int execute = 0;
int daemonize = 0;
int procs = 1;
int timeout = 30;

static void __dead
usage(void)
{
	fprintf(stderr, "fork-exit [-ed] [-p procs] [-t timeout]\n"
	    "    -e          child execs sleep(1), default call sleep(3)\n"
	    "    -d          daemonize, if already process group leader\n"
	    "    -p procs    number of processes to fork, default 1\n"
	    "    -t timeout  parent and children will exit, default 30 sec\n");
	exit(2);
}

static void __dead
exec_sleep(void)
{
	execl("/bin/sleep", "sleep", "30", NULL);
	err(1, "exec sleep");
}

static void
fork_sleep(int fd)
{
	switch (fork()) {
	case -1:
		err(1, "fork");
	case 0:
		break;
	default:
		return;
	}
	/* close pipe to parent and sleep until killed */
	if (execute) {
		if (fcntl(fd, F_SETFD, FD_CLOEXEC))
			err(1, "fcntl FD_CLOEXEC");
		exec_sleep();
	} else {
		if (close(fd) == -1)
			err(1, "close write");
		if (sleep(timeout) != 0)
			err(1, "sleep %d", timeout);
	}
	_exit(0);
}

static void
sigexit(int sig)
{
	int i, status;
	pid_t pid;

	/* all children must terminate in time */
	if ((int)alarm(timeout) == -1)
		err(1, "alarm");

	for (i = 0; i < procs; i++) {
		pid = wait(&status);
		if (pid == -1)
			err(1, "wait");
		if (!WIFSIGNALED(status))
			errx(1, "child %d not killed", pid);
		if(WTERMSIG(status) != SIGTERM)
			errx(1, "child %d signal %d", pid, WTERMSIG(status));
	}
	exit(0);
}

int
main(int argc, char *argv[])
{
	const char *errstr;
	int ch, i, fdmax, fdlen, *rfds, waiting;
	fd_set *fdset;
	pid_t pgrp;
	struct timeval tv;

	while ((ch = getopt(argc, argv, "edp:t:")) != -1) {
	switch (ch) {
		case 'e':
			execute = 1;
			break;
		case 'd':
			daemonize = 1;
			break;
		case 'p':
			procs = strtonum(optarg, 0, INT_MAX, &errstr);
			if (errstr != NULL)
				errx(1, "number of procs is %s: %s", errstr,
				    optarg);
			break;
		case 't':
			timeout = strtonum(optarg, 0, INT_MAX, &errstr);
			if (errstr != NULL)
				errx(1, "timeout is %s: %s", errstr, optarg);
		default:
			usage();
		}
	}

	/* become process group leader */
	pgrp = setsid();
	if (pgrp == -1) {
		if (errno == EPERM && daemonize) {
			/* get rid of leadership */
			switch (fork()) {
			case -1:
				err(1, "fork parent");
			case 0:
				/* try again */
				pgrp = setsid();
				break;
			default:
				_exit(0);
			}
		}
		if (!daemonize)
			warnx("try -d to become process group leader");
		if (pgrp == -1)
			err(1, "setsid");
	}

	/* create pipes to keep in contact with children */
	rfds = reallocarray(NULL, procs, sizeof(int));
	if (rfds == NULL)
		err(1, "rfds");
	fdmax = 0;

	/* fork child processes and pass writing end of pipe */
	for (i = 0; i < procs; i++) {
		int pipefds[2];

		if (pipe(pipefds) == -1)
			err(1, "pipe");
		if (fdmax < pipefds[0])
			fdmax = pipefds[0];
		rfds[i] = pipefds[0];
		fork_sleep(pipefds[1]);
		if (close(pipefds[1]) == -1)
			err(1, "close parent");
	}

	/* create select mask with all reading ends of child pipes */
	fdlen = howmany(fdmax + 1, NFDBITS);
	fdset = calloc(fdlen, sizeof(fd_mask));
	if (fdset == NULL)
		err(1, "fdset");
	for (i = 0; i < procs; i++) {
		FD_SET(rfds[i], fdset);
	}

	/* wait until all child processes are waiting */
	do  {
		waiting = 0;
		tv.tv_sec = timeout;
		tv.tv_usec = 0;
		errno = ETIMEDOUT;
		if (select(fdmax + 1, fdset, NULL, NULL, &tv) <= 0)
			err(1, "select");

		/* remove fd of children that closed their end  */
		for (i = 0; i < procs; i++) {
			if (rfds[i] >= 0) {
				if (FD_ISSET(rfds[i], fdset)) {
					if (close(rfds[i]) == -1)
						err(1, "close read");
					FD_CLR(rfds[i], fdset);
					rfds[i] = -1;
				} else {
					FD_SET(rfds[i], fdset);
					waiting = 1;
				}
			}
		}
	} while (waiting);

	/* kill all children simultaneously, parent exits in signal handler */
	if (signal(SIGTERM, sigexit) == SIG_ERR)
		err(1, "signal SIGTERM");
	if (kill(-pgrp, SIGTERM) == -1)
		err(1, "kill %d", -pgrp);

	errx(1, "alive");
}
