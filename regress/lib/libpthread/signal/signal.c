/*	$OpenBSD: signal.c,v 1.3 2001/11/11 23:26:35 deraadt Exp $	*/
/* David Leonard <d@openbsd.org>, 2001. Public Domain. */

/*
 * This program tests signal handler re-entrancy.
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include "test.h"

volatile int alarmed;

void *
sleeper(arg)
	void *arg;
{
	sigset_t mask;

	/* Ignore all signals in this thread */
	sigfillset(&mask);
	CHECKe(sigprocmask(SIG_SETMASK, &mask, NULL));
	ASSERT(sleep(3) == 0);
	SUCCEED;
}

void
handler(sig)
	int sig;
{
	int save_errno = errno;

	alarmed = 1;
	alarm(1);
	signal(SIGALRM, handler);
	errno = save_errno;
}

int
main()
{
	pthread_t slpr;

	ASSERT(signal(SIGALRM, handler) != SIG_ERR);
	CHECKe(alarm(1));
	CHECKr(pthread_create(&slpr, NULL, sleeper, NULL));
	/* ASSERT(sleep(1) == 0); */
	for (;;) {
		if (alarmed) {
			alarmed = 0;
			CHECKe(write(STDOUT_FILENO, "!", 1));
		}
	}
}
