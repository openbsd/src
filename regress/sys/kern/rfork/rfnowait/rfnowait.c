/*	$OpenBSD: rfnowait.c,v 1.1 2002/02/17 05:22:41 art Exp $	*/
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
main()
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