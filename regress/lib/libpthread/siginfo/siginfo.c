/* $OpenBSD: siginfo.c,v 1.6 2002/10/21 18:50:17 marc Exp $ */
/* PUBLIC DOMAIN Oct 2002 <marc@snafu.org> */

/*
 * test SA_SIGINFO support.   Also check that SA_RESETHAND does the right
 * thing.
 */

#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include "test.h"

#define BOGUS	(char *)0x987230

void
act_handler(int signal, siginfo_t *siginfo, void *context)
{
	struct sigaction sa;
	char * str;

	CHECKe(sigaction(SIGSEGV, NULL, &sa));
	ASSERT(sa.sa_handler == SIG_DFL);
	ASSERT(siginfo != NULL);
	asprintf(&str, "act_handler: signal %d, siginfo 0x%p, context 0x%p\n"
		 "addr 0x%p, code %d, trap %d\n", signal, siginfo, context,
		 siginfo->si_addr, siginfo->si_code, siginfo->si_trapno);
	write(STDOUT_FILENO, str, strlen(str));
 	ASSERT(siginfo->si_addr == BOGUS &&
	       siginfo->si_code == 1 && siginfo->si_trapno == 2);
	SUCCEED;
}
 
int
main(int argc, char **argv)
{
	struct sigaction act;

	act.sa_sigaction = act_handler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_SIGINFO | SA_RESETHAND | SA_NODEFER;
	CHECKe(sigaction(SIGSEGV, &act, NULL));
	*BOGUS = 1;
	PANIC("How did we get here?");
}
