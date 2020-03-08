/*	$OpenBSD: kqueue-regress.c,v 1.4 2020/03/08 09:40:52 visa Exp $	*/
/*
 *	Written by Anton Lindqvist <anton@openbsd.org> 2018 Public Domain
 */

#include <sys/types.h>
#include <sys/event.h>
#include <sys/resource.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <assert.h>
#include <err.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "main.h"

static int do_regress1(void);
static int do_regress2(void);
static int do_regress3(void);
static int do_regress4(void);
static int do_regress5(void);

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
	case 4:
		return do_regress4();
	case 5:
		return do_regress5();
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

/*
 * Regression test for kernel stack exhaustion.
 */
static int
do_regress4(void)
{
	static const int nkqueues = 500;
	struct kevent kev[1];
	struct rlimit rlim;
	struct timespec ts;
	int fds[2], i, kq = -1, prev;

	if (getrlimit(RLIMIT_NOFILE, &rlim) == -1)
		err(1, "getrlimit");
	if (rlim.rlim_cur < nkqueues + 8) {
		rlim.rlim_cur = nkqueues + 8;
		if (setrlimit(RLIMIT_NOFILE, &rlim) == -1) {
			printf("RLIMIT_NOFILE is too low and can't raise it\n");
			printf("SKIPPED\n");
			exit(0);
		}
	}

	if (pipe(fds) == -1)
		err(1, "pipe");

	/* Build a chain of kqueus. The first kqueue refers to the pipe. */
	for (i = 0, prev = fds[0]; i < nkqueues; i++, prev = kq) {
		kq = kqueue();
		if (kq == -1)
			err(1, "kqueue");

		EV_SET(&kev[0], prev, EVFILT_READ, EV_ADD, 0, 0, NULL);
		if (kevent(kq, kev, 1, NULL, 0, NULL) == -1)
			err(1, "kevent");
	}

	/*
	 * Trigger a cascading event through the chain.
	 * If the chain is long enough, a broken kernel can run out
	 * of kernel stack space.
	 */
	write(fds[1], "x", 1);

	/*
	 * Check that the event gets propagated.
	 * The propagation is not instantaneous, so allow a brief pause.
	 */
	ts.tv_sec = 5;
	ts.tv_nsec = 0;
	assert(kevent(kq, NULL, 0, kev, 1, NULL) == 1);

	return 0;
}

/*
 * Regression test for select and poll with kqueue.
 */
static int
do_regress5(void)
{
	fd_set fdset;
	struct kevent kev[1];
	struct pollfd pfd[1];
	struct timeval tv;
	int fds[2], kq, ret;

	if (pipe(fds) == -1)
		err(1, "pipe");

	kq = kqueue();
	if (kq == -1)
		err(1, "kqueue");
	EV_SET(&kev[0], fds[0], EVFILT_READ, EV_ADD, 0, 0, NULL);
	if (kevent(kq, kev, 1, NULL, 0, NULL) == -1)
		err(1, "kevent");

	/* Check that no event is reported. */

	FD_ZERO(&fdset);
	FD_SET(kq, &fdset);
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	ret = select(kq + 1, &fdset, NULL, NULL, &tv);
	if (ret == -1)
		err(1, "select");
	assert(ret == 0);

	pfd[0].fd = kq;
	pfd[0].events = POLLIN;
	pfd[0].revents = 0;
	ret = poll(pfd, 1, 0);
	if (ret == -1)
		err(1, "poll");
	assert(ret == 0);

	/* Trigger an event. */
	write(fds[1], "x", 1);

	/* Check that the event gets reported. */

	FD_ZERO(&fdset);
	FD_SET(kq, &fdset);
	tv.tv_sec = 5;
	tv.tv_usec = 0;
	ret = select(kq + 1, &fdset, NULL, NULL, &tv);
	if (ret == -1)
		err(1, "select");
	assert(ret == 1);
	assert(FD_ISSET(kq, &fdset));

	pfd[0].fd = kq;
	pfd[0].events = POLLIN;
	pfd[0].revents = 0;
	ret = poll(pfd, 1, 5000);
	if (ret == -1)
		err(1, "poll");
	assert(ret == 1);
	assert(pfd[0].revents & POLLIN);

	return 0;
}
