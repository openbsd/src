/*	$OpenBSD: ignore_sigchild.c,v 1.1 2011/10/09 23:46:50 fgsch Exp $	*/
/*
 * Federico G. Schwindt <fgsch@openbsd.org>, 2011. Public Domain.
 */
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include "test.h"

int
main(int argc, char **argv)
{
	int status;
	pid_t pid;

	ASSERT(signal(SIGCHLD, SIG_IGN) != SIG_ERR);

	switch ((pid = fork())) {
	case -1:
		PANIC("fork");
	case 0:
		execl("/usr/bin/false", "false", NULL);
		PANIC("execlp");
	default:
		break;
	}

	ASSERT(wait(&status) == -1);
	ASSERT(errno == ECHILD);
	SUCCEED;
}
