/*	$OpenBSD: toint.c,v 1.6 2009/07/06 00:06:10 martynas Exp $	*/

/*	Written by Michael Shalayeff, 2003, Public domain.	*/

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

void
sigfpe(int sig, siginfo_t *si, void *v)
{
	char buf[132];

	if (si) {
		snprintf(buf, sizeof(buf), "sigfpe: trap=%d code=%d addr=%p\n",
		    si->si_trapno, si->si_code, si->si_addr);
		write(1, buf, strlen(buf));
	}
	_exit(1);
}

int
toint(double d)
{
	return (int)(d + 1);
}

int
main(int argc, char *argv[])
{
	struct sigaction sa;
	int i;

	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = sigfpe;
	sa.sa_flags = SA_SIGINFO;
	sigaction(SIGFPE, &sa, NULL);

	if (toint(8.6) != 9)
		exit(1);

	exit(0);
}
