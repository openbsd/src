/*	$OpenBSD: rint.c,v 1.1 2003/02/12 07:05:34 mickey Exp $	*/

/*	Copyright (c) 2003 Michael Shalayeff. Public domain.	*/

#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <math.h>

void
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
main()
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = sigfpe;
	sa.sa_flags = SA_SIGINFO;
	sigaction(SIGFPE, &sa, NULL);

	if (rint(8.6) != 9.)
		exit(1);

	exit(0);
}
