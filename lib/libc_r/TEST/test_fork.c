/* ==== test_fork.c ============================================================
 * Copyright (c) 1994 by Chris Provenzano, proven@athena.mit.edu
 *
 * Description : Test fork() and dup2() calls.
 *
 *  1.00 94/04/29 proven
 *      -Started coding this file.
 */

#include <pthread.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include "test.h"

int
main()
{
	int flags;
	pid_t pid;
	extern int _thread_sys_fcntl __P((int, int, ...));

	if (((flags = _thread_sys_fcntl(1, F_GETFL, NULL)) >= OK) && 
	  (flags & (O_NONBLOCK | O_NDELAY))) {
		_thread_sys_fcntl(1, F_SETFL, flags & ~(O_NONBLOCK | O_NDELAY));
	}
	printf("parent process %d\n", getpid());

	switch(pid = fork()) {
	case OK:
		exit(OK);
	case NOTOK:
		printf("fork() FAILED\n");
		exit(2);
	default:
		printf("child process %d\n", pid);
		break;
	}

	printf("test_fork PASSED\n");

	return 0;
}
