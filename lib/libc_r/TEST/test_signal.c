/* $OpenBSD: test_signal.c,v 1.1 2000/01/04 00:17:46 d Exp $ */

/*
 * This program tests signal handler re-entrancy.
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include "test.h"

void *
sleeper(arg)
	void *arg;
{
	sigset_t mask;

	/* Ignore all signals in this thread */
	sigfillset(&mask);
	CHECKe(sigprocmask(SIG_SETMASK, &mask, NULL));

	ASSERT(sleep(10) == 0);
	SUCCEED;
}

void
handler(sig)
	int sig;
{
	printf("signal handler %d\n", sig);
	alarm(1);
	signal(SIGALRM, handler);
}

int
main()
{
	pthread_t slpr;

	ASSERT(signal(SIGALRM, handler) != SIG_ERR);
	CHECKe(alarm(1));
	/* CHECKr(pthread_create(&slpr, NULL, sleeper, NULL)); */
	/* ASSERT(sleep(1) == 0); */
	for (;;)
		CHECKe(write(STDOUT_FILENO, ".", 1));
}
