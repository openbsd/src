/*	$OpenBSD: signal.c,v 1.2 2001/11/03 04:33:48 marc Exp $	*/
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
	alarmed = 1;
	alarm(1);
	signal(SIGALRM, handler);
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
