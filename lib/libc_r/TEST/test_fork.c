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
	sleep(10);
	PANIC("sleeper timed out");
}

static pid_t parent_pid;

static void
sigchld(sig)
	int sig;
{
	int status;

	/* we should have got a SIGCHLD */
	ASSERT(sig == SIGCHLD);
	/* We should be the parent */
	ASSERT(getpid() == parent_pid);
	/* wait for any child */
	CHECKe(wait(&status));
	/* the child should have called exit(0) */
	ASSERT(WIFEXITED(status));
	ASSERT(WEXITSTATUS(status) == 0);
	printf("parent ok\n");
	SUCCEED;
}

static int atfork_state = 0;

void
atfork_child2()
{
	ASSERT(atfork_state++ == 3);
	ASSERT(getpid() != parent_pid);
}

void
atfork_parent2()
{
	ASSERT(atfork_state++ == 3);
	ASSERT(getpid() == parent_pid);
}

void
atfork_prepare2()
{
	ASSERT(atfork_state++ == 0);
	ASSERT(getpid() == parent_pid);
}


void
atfork_child1()
{
	ASSERT(atfork_state++ == 2);
	ASSERT(getpid() != parent_pid);
}

void
atfork_parent1()
{
	ASSERT(atfork_state++ == 2);
	ASSERT(getpid() == parent_pid);
}

void
atfork_prepare1()
{
	ASSERT(atfork_state++ == 1);
	ASSERT(getpid() == parent_pid);
}

int
main()
{
	int flags;
	pid_t pid;
	pthread_t sleeper_thread;

	parent_pid = getpid();

	CHECKe(flags = fcntl(STDOUT_FILENO, F_GETFL));
	if ((flags & (O_NONBLOCK | O_NDELAY))) {
		CHECKe(fcntl(STDOUT_FILENO, F_SETFL, 
		    flags & ~(O_NONBLOCK | O_NDELAY)));
	}

	CHECKr(pthread_create(&sleeper_thread, NULL, sleeper, NULL));
	sleep(1);

	CHECKe(signal(SIGCHLD, sigchld));

	/* Install some atfork handlers */

	CHECKr(pthread_atfork(&atfork_prepare1, &atfork_parent1, 
		&atfork_child1));
	CHECKr(pthread_atfork(&atfork_prepare2, &atfork_parent2, 
		&atfork_child2));

	printf("forking\n");

	CHECKe(pid = fork());
	switch(pid) {
	case 0:
		/* child: */
		/* Our pid should change */
		ASSERT(getpid() != parent_pid);
		/* Our sleeper thread should have disappeared */
		ASSERT(ESRCH == pthread_cancel(sleeper_thread));
		/* The atfork handler should have run */
		ASSERT(atfork_state++ == 4);
		printf("child ok\n");
		_exit(0);
		PANIC("child _exit");
	default:
		/* parent: */
		/* Our pid should stay the same */
		ASSERT(getpid() == parent_pid);
		/* Our sleeper thread should still be around */
		CHECKr(pthread_cancel(sleeper_thread));
		/* The atfork handler should have run */
		ASSERT(atfork_state++ == 4);
		/* wait for the SIGCHLD from the child */
		CHECKe(pause());
		PANIC("pause");
	}
}
