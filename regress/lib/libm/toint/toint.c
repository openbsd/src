/*	$OpenBSD: toint.c,v 1.1 2003/02/12 07:08:44 mickey Exp $	*/

/*	Copyright (c) 2003 Michael Shalayeff. Publci domain.	*/

#include <stdio.h>
#include <signal.h>
#include <unistd.h>

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
toint(double d)
{
	return (int)d;
}

int
main()
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = sigfpe;
	sa.sa_flags = SA_SIGINFO;
	sigaction(SIGFPE, &sa, NULL);

	if (toint(8.6) != 8)
		exit(1);

	exit(0);
}
