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
#include <err.h>
#include "test.h"

extern char **environ;
char *argv[] = {
	"/bin/echo",
	"This message should be displayed after the execve system call",
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

		ttynm = ttyname(1);
		printf("tty is %s\n", ttynm);
		if ((fd = open(ttynm, O_RDWR)) < OK)
			err(1, "%s", ttynm);
	} else
		errx(1, "stdout not a tty");

	printf("This output is necessary to set the stdout fd to NONBLOCKING\n");

	/* do a dup2 */
	dup2(fd, 1);
	write(1, should_succeed, (size_t)strlen(should_succeed));
#if i_understood_this
	_thread_sys_write(1, should_fail, strlen(should_fail));
#endif
	if (execve(argv[0], argv, environ) < OK) 
		err(1, "execve");

	PANIC();
}
