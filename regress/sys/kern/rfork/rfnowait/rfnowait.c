/*	$OpenBSD: rfnowait.c,v 1.3 2003/07/31 21:48:09 deraadt Exp $	*/
/*
 * Written by Artur Grabowski <art@openbsd.org>, 2002 Public Domain.
 */
#include <sys/param.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <errno.h>

int
main(int argc, char *argv[])
{
	int status;

	switch(rfork(RFFDG|RFNOWAIT|RFPROC)) {
	case -1:
		err(1, "fork");
	case 0:
		_exit(0);
	}

	if (wait(&status) >= 0)
		errx(1, "wait returned a child?");

	if (errno != ECHILD)
		err(1, "unexpected errno (%d)");

	return 0;
}
