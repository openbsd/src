/* $OpenBSD: siginfo.c,v 1.5 2002/10/12 03:37:45 marc Exp $ */
/* PUBLIC DOMAIN Oct 2002 <marc@snafu.org> */

/*
 * test SA_SIGINFO support.   Also check that SA_RESETHAND does the right
 * thing.
 */

#include <signal.h>
#include <stdio.h>

#include "test.h"

void
act_handler(int signal, siginfo_t *siginfo, void *context)
{
	struct sigaction sa;

	CHECKe(sigaction(SIGSEGV, NULL, &sa));
	ASSERT(sa.sa_handler == SIG_DFL);
	ASSERT(siginfo != NULL);
 	ASSERT(siginfo->si_addr == (char *) 0x987234 &&
	       siginfo->si_code == 1 && siginfo->si_trapno == 2);
}
 
int
main(int argc, char **argv)
{
	struct sigaction act;

	act.sa_sigaction = act_handler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_SIGINFO | SA_RESETHAND;
	CHECKe(sigaction(SIGSEGV, &act, NULL));
	*(char *) 0x987234 = 1;
	SUCCEED;
}
