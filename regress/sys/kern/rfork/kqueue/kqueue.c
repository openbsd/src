/*	$OpenBSD: kqueue.c,v 1.1 2010/08/04 06:05:26 guenther Exp $	*/
/*
 * Written by Philip Guenther <guenther@openbsd.org>, 2010 Public Domain.
 *
 * Verify that having a process exit while it has knotes attached to it
 * that are from a kqueue that is open in another process doesn't cause
 * problems.
 */
#include <sys/param.h>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <err.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
	struct kevent ev;
	int status;
	int kq;

	if ((kq = kqueue()) < 0)
		err(1, "kqueue");

	signal(SIGINT, SIG_IGN);
	EV_SET(&ev, SIGINT, EVFILT_SIGNAL, EV_ADD|EV_ENABLE, 0, 0, 0);

	switch(rfork(RFPROC)) {
	case -1:
		err(1, "rfork");
	case 0:
		if (kevent(kq, &ev, 1, NULL, 0, NULL))
			err(1, "kevent");
		raise(SIGINT);
		_exit(0);
	}

	if (wait(&status) < 0)
		err(1, "wait");

	if (!WIFEXITED(status))
		err(1, "child error");

	return WEXITSTATUS(status) != 0;
}
