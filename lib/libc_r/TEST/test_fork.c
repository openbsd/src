/* ==== test_fork.c ============================================================
 * Copyright (c) 1994 by Chris Provenzano, proven@athena.mit.edu
 *
 * Description : Test fork() and dup2() calls.
 *
 *  1.00 94/04/29 proven
 *      -Started coding this file.
 */

#include <pthread.h>
#include <pthread_np.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include "test.h"

void *
sleeper(void *arg)
{
	pthread_set_name_np(pthread_self(), "slpr");
	printf("sleeper\n");
	sleep(10);
	PANIC("sleeper timed out");
}

static void
sigchld(sig)
	int sig;
{
	int status;

	ASSERT(sig == SIGCHLD);
	CHECKe(wait(&status));
	ASSERT(WIFEXITED(status));
	ASSERT(WEXITSTATUS(status) == 0);
	SUCCEED;
}

int
main()
{
	int flags;
	pid_t pid;
	pthread_t sleeper_thread;

	CHECKe(flags = fcntl(STDOUT_FILENO, F_GETFL));
	if ((flags & (O_NONBLOCK | O_NDELAY))) {
		CHECKe(fcntl(STDOUT_FILENO, F_SETFL, 
		    flags & ~(O_NONBLOCK | O_NDELAY)));
	}

	CHECKr(pthread_create(&sleeper_thread, NULL, sleeper, NULL));
	sleep(1);

	CHECKe(signal(SIGCHLD, sigchld));

	printf("forking\n");

	CHECKe(pid = fork());
	switch(pid) {
	case 0:
		sleep(1);
		printf("child process %d\n", getpid());
		_thread_dump_info();
		printf("\n");
		_exit(0);
		PANIC("_exit");
	default:
		printf("parent process %d [child %d]\n", getpid(), pid);
		_thread_dump_info();
		printf("\n");
		CHECKe(pause());
		PANIC("pause");
	}
}
