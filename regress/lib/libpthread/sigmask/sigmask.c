/* $OpenBSD: sigmask.c,v 1.1 2003/07/10 21:02:12 marc Exp $ */
/* PUBLIC DOMAIN July 2003 Marco S Hyman <marc@snafu.org> */

#include <sys/time.h>

#include <pthread.h>
#include <signal.h>
#include <unistd.h>

#include "test.h"

/*
 * Test that masked signals with a default action of terminate process
 * do NOT terminate the process.
 */
int main (int argc, char *argv[])
{
	sigset_t mask;

	/* mask sigalrm */
	CHECKe(sigemptyset(&mask));
	CHECKe(sigaddset(&mask, SIGALRM));
	CHECKe(pthread_sigmask(SIG_BLOCK, &mask, NULL));

	/* now trigger sigalrm */
	ualarm(100000, 0);

	/* wait for it -- we shouldn't see it. */
	CHECKe(sleep(1));

	SUCCEED;
}
