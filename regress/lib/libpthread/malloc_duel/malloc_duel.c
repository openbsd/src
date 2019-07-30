/* $OpenBSD: malloc_duel.c,v 1.2 2003/07/31 21:48:04 deraadt Exp $ */
/* PUBLIC DOMAIN Nov 2002 <marc@snafu.org> */

/*
 * Dueling malloc in different threads
 */

#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include "test.h"

volatile sig_atomic_t	done;

#define MALLOC_COUNT	1024

/*
 * sigalrm handler.  Initiate end-of-test
 */
static void
alarm_handler(int sig)
{
	done = 1;
}

/*
 * A function that does lots of mallocs, called by all threads.
 */
static void
malloc_loop(void)
{
	int	i;
	int	**a;

	a = calloc(MALLOC_COUNT, sizeof(int*));
	ASSERT(a != NULL);
	while (!done) {
		for (i = 0; i < MALLOC_COUNT; i++) {
			a[i] = malloc(sizeof(int));
			ASSERT(a[i] != NULL);
		}
		for (i = 0; i < MALLOC_COUNT; i++) {
			free(a[i]);
		}
	}
}

/*
 * A thread that does a lot of mallocs
 */
static void *
thread(void *arg)
{
	malloc_loop();
	return NULL;
}

int
main(int argc, char **argv)
{
	pthread_t	child;

	CHECKr(pthread_create(&child, NULL, thread, NULL));
	ASSERT(signal(SIGALRM, alarm_handler) != SIG_ERR);
	CHECKe(alarm(20));
	malloc_loop();
	CHECKr(pthread_join(child, NULL));
	SUCCEED;
}
