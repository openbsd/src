/*	$OpenBSD: kqueue-fork.c,v 1.2 2003/07/31 21:48:08 deraadt Exp $	*/
/*
 *	Written by Artur Grabowski <art@openbsd.org> 2002 Public Domain
 */

#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/event.h>
#include <sys/wait.h>

int check_inheritance(void);

int
check_inheritance(void)
{
	int kq, status;

	if ((kq = kqueue()) < 0) {
		warn("kqueue");
		return (1);
	}

	/*
	 * Check if the kqueue is properly closed on fork().
	 */

	switch (fork()) {
	case -1:
		err(1, "fork");
	case 0:
		if (close(kq) < 0)
			_exit(0);
		warnx("fork didn't close kqueue");
		_exit(1);
	}
	if (wait(&status) < 0)
		err(1, "wait");

	if (!WIFEXITED(status))
		errx(1, "child didn't exit?");

	close(kq);
	return (WEXITSTATUS(status) != 0);
}
