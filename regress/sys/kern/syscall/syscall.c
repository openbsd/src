/*	$OpenBSD: syscall.c,v 1.1 2002/02/08 21:33:32 art Exp $	*/
/*
 *	Written by Artur Grabowski <art@openbsd.org> 2002 Public Domain.
 */
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <err.h>

int
main()
{
	int status;

	switch(fork()) {
	case -1:
		err(1, "fork");
	case 0:
		syscall(SYS_exit, 17);
		abort();
	}

	if (wait(&status) < 0)
		err(1, "wait");

	if (!WIFEXITED(status))
		errx(1, "child didn't exit gracefully");

	if (WEXITSTATUS(status) != 17)
		errx(1, "wrong exit status");

	return 0;
}
