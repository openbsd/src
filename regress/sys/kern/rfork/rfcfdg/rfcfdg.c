/*	$OpenBSD: rfcfdg.c,v 1.3 2003/07/31 21:48:09 deraadt Exp $	*/
/*
 * Written by Artur Grabowski <art@openbsd.org>, 2002 Public Domain.
 */
#include <sys/param.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <fcntl.h>

int
main(int argc, char *argv[])
{
	int status;

	/* Make sure that at least fd 0 is allocated. */
	if (open("/dev/null", O_RDONLY) < 0)
		err(1, "open(/dev/null)");

	switch(rfork(RFCFDG|RFPROC)) {
	case -1:
		err(1, "fork");
	case 0:
		if (open("/dev/null", O_RDONLY) != 0)
			_exit(1);	/* pretty hard to print to stderr */
		_exit(0);
	}

	if (wait(&status) < 0)
		err(1, "wait");

	if (!WIFEXITED(status))
		err(1, "child error");

	return WEXITSTATUS(status) != 0;
}
