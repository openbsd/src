/*	$OpenBSD: rfsharefd.c,v 1.1 2002/02/17 05:44:07 art Exp $	*/
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
main()
{
	int status;

	/* Make sure that at least fd 0 is allocated. */
	if (open("/dev/null", O_RDONLY) < 0)
		err(1, "open(/dev/null)");

	switch(rfork(RFPROC)) {
	case -1:
		err(1, "fork");
	case 0:
		if (close(0) < 0)
			_exit(1);
		_exit(0);
	}

	if (wait(&status) < 0)
		err(1, "wait");

	if (!WIFEXITED(status))
		err(1, "child error");

	if (close(0) == 0)
		errx(1, "fd 0 not closed");

	return WEXITSTATUS(status) != 0;
}