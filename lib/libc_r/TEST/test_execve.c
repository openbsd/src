/*	$OpenBSD: test_execve.c,v 1.5 2000/10/04 05:50:58 d Exp $	*/
/*
 * Copyright (c) 1994 by Chris Provenzano, proven@athena.mit.edu
 *
 * Test execve() and dup2() calls.
 */

#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include "test.h"

extern char **environ;
char *argv[] = {
	"/bin/echo",
	"This line should appear after the execve",
	NULL
};

char * should_succeed = "This line should be displayed\n";

int
main()
{
	int fd;

	printf("This is the first message\n");
	if (isatty(STDOUT_FILENO)) {
		char *ttynm;

		CHECKn(ttynm = ttyname(STDOUT_FILENO));
		printf("tty is %s\n", ttynm);
		CHECKe(fd = open(ttynm, O_RDWR));
	} else
		PANIC("stdout is not a tty: this test needs a tty");

	CHECKn(printf("This output is necessary to set the stdout fd to NONBLOCKING\n"));

	/* do a dup2 */
	CHECKe(dup2(fd, STDOUT_FILENO));
	CHECKe(write(STDOUT_FILENO, should_succeed,
	    (size_t)strlen(should_succeed)));
	CHECKe(execve(argv[0], argv, environ));
	DIE(errno, "execve %s", argv[0]);
}
