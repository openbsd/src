/*	$OpenBSD: madness.c,v 1.3 2003/08/02 01:24:36 david Exp $	*/
/*
 *	Written by Artur Grabowski <art@openbsd.org>, 2002 Public Domain.
 */
#include <sys/param.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <err.h>

volatile int step;

int
main(int argc, char *argv[])
{
	int fds[2], fd;
	pid_t pid1, pid2, pid3;

	if (pipe(fds) < 0)
		err(1, "pipe");

	fd = fds[0];

	if ((pid1 = rfork(RFPROC|RFMEM|RFFDG|RFNOWAIT)) == 0) {
		char foo[1024];
		step = 1;
		read(fd, foo, sizeof(foo));
		_exit(0);
	}
        
	if ((pid2 = rfork(RFPROC|RFMEM|RFFDG|RFNOWAIT)) == 0) {
		while (step < 1)
			sleep(1);
		sleep(1);
		step = 2;
		close(fd);
		_exit(0);
	}
	if ((pid3 = rfork(RFPROC|RFMEM|RFFDG|RFNOWAIT)) == 0) {
		while (step < 2)
			sleep(1);
		sleep(1);
		step = 3;
		dup2(0, fd);
		_exit(0);
	}
	while (step < 3)
		sleep(1);
	sleep(1);
	kill(pid1, SIGKILL);
	kill(pid2, SIGKILL);
	kill(pid3, SIGKILL);
	return 0;
}
