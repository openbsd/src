/*	$OpenBSD: wait.c,v 1.1 2003/11/04 07:38:08 mickey Exp $	*/
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
		__syscall((u_int64_t)SYS_exit, 17, 0);
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
