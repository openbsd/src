/* ==== test_execve.c ============================================================
 * Copyright (c) 1994 by Chris Provenzano, proven@athena.mit.edu
 *
 * Description : Test execve() and dup2() calls.
 *
 *  1.00 94/04/29 proven
 *      -Started coding this file.
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
char * should_fail = "Error: This line should NOT be displayed\n";

int
main()
{
	int fd;

	printf("This is the first message\n");
	if (isatty(1)) {
		char *ttynm;

		CHECKn(ttynm = ttyname(1));
		printf("tty is %s\n", ttynm);
		CHECKe(fd = open(ttynm, O_RDWR));
	} else
		PANIC("stdout not a tty");

	CHECKn(printf("This output is necessary to set the stdout fd to NONBLOCKING\n"));

	/* do a dup2 */
	CHECKe(dup2(fd, 1));
	CHECKe(write(1, should_succeed, (size_t)strlen(should_succeed)));
	CHECKe(execve(argv[0], argv, environ));
	DIE(errno, "execve %s", argv[0]);
}
