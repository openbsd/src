/*	$OpenBSD: test_fork.c,v 1.7 2000/01/06 06:54:28 d Exp $	*/
/*
 * Copyright (c) 1994 by Chris Provenzano, proven@athena.mit.edu
 *
 * Test the fork system call.
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
	sleep(4);
	PANIC("sleeper timed out");
}


int
main()
{
	int flags;
	pthread_t sleeper_thread;
	void *result;
	int status;
	pid_t parent_pid;
	pid_t child_pid;

	parent_pid = getpid();

	CHECKe(flags = fcntl(STDOUT_FILENO, F_GETFL));
	if ((flags & (O_NONBLOCK | O_NDELAY))) {
		CHECKe(fcntl(STDOUT_FILENO, F_SETFL, 
		    flags & ~(O_NONBLOCK | O_NDELAY)));
	}

	CHECKr(pthread_create(&sleeper_thread, NULL, sleeper, NULL));
	sleep(1);

	printf("forking from pid %d\n", getpid());

	CHECKe(child_pid = fork());
	if (child_pid == 0) {
		/* child: */
		printf("child = pid %d\n", getpid());
		/* Our pid should change */
		ASSERT(getpid() != parent_pid);
		/* Our sleeper thread should have disappeared */
		ASSERT(ESRCH == pthread_join(sleeper_thread, &result));
		printf("child ok\n");
		_exit(0);
		PANIC("child _exit");
	}

	/* parent: */
	printf("parent = pid %d\n", getpid());
	/* Our pid should stay the same */
	ASSERT(getpid() == parent_pid);
	/* wait for the child */
	ASSERTe(wait(&status), == child_pid);
	/* the child should have called exit(0) */
	ASSERT(WIFEXITED(status));
	ASSERT(WEXITSTATUS(status) == 0);
	/* Our sleeper thread should still be around */
	CHECKr(pthread_detach(sleeper_thread));
	printf("parent ok\n");
	SUCCEED;
}
