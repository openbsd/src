/*	$OpenBSD: rint.c,v 1.7 2008/12/09 20:35:13 martynas Exp $	*/

/*	Written by Michael Shalayeff, 2003,  Public domain.	*/

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <math.h>

static void
sigfpe(int sig, siginfo_t *si, void *v)
{
	char buf[132];

	if (si) {
		snprintf(buf, sizeof(buf), "sigfpe: addr=%p, code=%d\n",
		    si->si_addr, si->si_code);
		write(1, buf, strlen(buf));
	}
	_exit(1);
}

int
main(int argc, char *argv[])
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = sigfpe;
	sa.sa_flags = SA_SIGINFO;
	sigaction(SIGFPE, &sa, NULL);

	if (rint(8.6) != 9.)
		errx(1, "rint");
	if (rintf(8.6F) != 9)
		errx(1, "rintf");
	if (rintl(8.6L) != 9)
		errx(1, "rintl");
 	if (lrint(8.6) != 9L)
 		errx(1, "lrint");
 	if (lrintf(8.6F) != 9L)
 		errx(1, "lrintf");
 	if (llrint(8.6) != 9LL)
 		errx(1, "llrint");
 	if (llrintf(8.6F) != 9LL)
 		errx(1, "llrintf");

	exit(0);
}
