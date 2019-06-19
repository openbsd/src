/*	$OpenBSD: kqueue-regress.c,v 1.3 2018/08/24 12:46:39 visa Exp $	*/
/*
 *	Written by Anton Lindqvist <anton@openbsd.org> 2018 Public Domain
 */

#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <assert.h>
#include <err.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include "main.h"

static int do_regress1(void);
static int do_regress2(void);
static int do_regress3(void);

static void make_chain(int);

int
do_regress(int n)
{
	switch (n) {
	case 1:
		return do_regress1();
	case 2:
		return do_regress2();
	case 3:
		return do_regress3();
	default:
		errx(1, "unknown regress test number %d", n);
	}
}

/*
 * Regression test for NULL-deref in knote_processexit().
 */
static int
do_regress1(void)
{
	struct kevent kev[2];
	int kq;

	ASS((kq = kqueue()) >= 0,
	    warn("kqueue"));

	EV_SET(&kev[0], kq, EVFILT_READ, EV_ADD, 0, 0, NULL);
	EV_SET(&kev[1], SIGINT, EVFILT_SIGNAL, EV_ADD, 0, 0, NULL);
	ASS(kevent(kq, kev, 2, NULL, 0, NULL) == 0,
	    warn("can't register events on kqueue"));

	/* kq intentionally left open */

	return 0;
}

/*
 * Regression test for use-after-free in kqueue_close().
 */
static int
do_regress2(void)
{
	pid_t pid;
	int i, status;

	/* Run twice in order to trigger the panic faster, if still present. */
	for (i = 0; i < 2; i++) {
		pid = fork();
		if (pid == -1)
			err(1, "fork");

		if (pid == 0) {
			struct kevent kev[1];
			int p0[2], p1[2];
			int kq;

			if (pipe(p0) == -1)
				err(1, "pipe");
			if (pipe(p1) == -1)
				err(1, "pipe");

			kq = kqueue();
			if (kq == -1)
				err(1, "kqueue");

			EV_SET(&kev[0], p0[0], EVFILT_READ, EV_ADD, 0, 0, NULL);
			if (kevent(kq, kev, 1, NULL, 0, NULL) == -1)
				err(1, "kevent");

			EV_SET(&kev[0], p1[1], EVFILT_READ, EV_ADD, 0, 0, NULL);
			if (kevent(kq, kev, 1, NULL, 0, NULL) == -1)
				err(1, "kevent");

			EV_SET(&kev[0], p1[0], EVFILT_READ, EV_ADD, 0, 0, NULL);
			if (kevent(kq, kev, 1, NULL, 0, NULL) == -1)
				err(1, "kevent");

			_exit(0);
		}

		if (waitpid(pid, &status, 0) == -1)
			err(1, "waitpid");
		assert(WIFEXITED(status));
		assert(WEXITSTATUS(status) == 0);
	}

	return 0;
}

/*
 * Regression test for kernel stack exhaustion.
 */
static int
do_regress3(void)
{
	pid_t pid;
	int dir, status;

	for (dir = 0; dir < 2; dir++) {
		pid = fork();
		if (pid == -1)
			err(1, "fork");

		if (pid == 0) {
			make_chain(dir);
			_exit(0);
		}

		if (waitpid(pid, &status, 0) == -1)
			err(1, "waitpid");
		assert(WIFEXITED(status));
		assert(WEXITSTATUS(status) == 0);
	}

	return 0;
}

static void
make_chain(int dir)
{
	struct kevent kev[1];
	int i, kq, prev;

	/*
	 * Build a chain of kqueues and leave the files open.
	 * If the chain is long enough and properly oriented, a broken kernel
	 * can exhaust the stack when this process exits.
	 */
	for (i = 0, prev = -1; i < 120; i++, prev = kq) {
		kq = kqueue();
		if (kq == -1)
			err(1, "kqueue");
		if (prev == -1)
			continue;

		if (dir == 0) {
			EV_SET(&kev[0], prev, EVFILT_READ, EV_ADD, 0, 0, NULL);
			if (kevent(kq, kev, 1, NULL, 0, NULL) == -1)
				err(1, "kevent");
		} else {
			EV_SET(&kev[0], kq, EVFILT_READ, EV_ADD, 0, 0, NULL);
			if (kevent(prev, kev, 1, NULL, 0, NULL) == -1)
				err(1, "kevent");
		}
	}
}
