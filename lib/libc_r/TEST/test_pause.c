/*	$OpenBSD: test_pause.c,v 1.3 2000/01/06 06:55:13 d Exp $	*/
/*
 * Test pause() 
 */
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include "test.h"

int gotsig = 0;

void
handler(sig) 
	int sig;
{
	printf("%s\n", strsignal(sig));
}

int
main()
{
	sigset_t all;
	pid_t self;

	ASSERT(signal(SIGHUP, handler) != SIG_ERR);
	CHECKe(self = getpid());
	CHECKe(sigemptyset(&all));
	CHECKe(sigaddset(&all, SIGHUP));
	CHECKe(sigprocmask(SIG_BLOCK, &all, NULL));
	CHECKe(kill(self, SIGHUP));
	CHECKe(pause());
	SUCCEED;
}
