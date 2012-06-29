/*	$OpenBSD: sigpending.c,v 1.1 2012/06/29 00:21:55 matthew Exp $	*/
/*
 * Written by Matthew Dempsky, 2011.
 * Public domain.
 */

#include <assert.h>
#include <signal.h>

int
main()
{
	sigset_t set;

	assert(sigemptyset(&set) == 0);
	assert(sigaddset(&set, SIGUSR1) == 0);
	assert(sigprocmask(SIG_BLOCK, &set, NULL) == 0);
	assert(raise(SIGUSR1) == 0);
	assert(sigpending(&set) == 0);
	assert(sigismember(&set, SIGUSR1) == 1);

	return (0);
}
