/*	$OpenBSD: fork.c,v 1.4 2003/07/31 21:48:04 deraadt Exp $	*/
/*
 * Copyright (c) 1993, 1994, 1995, 1996 by Chris Provenzano and contributors, 
 * proven@mit.edu All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Chris Provenzano,
 *	the University of California, Berkeley, and contributors.
 * 4. Neither the name of Chris Provenzano, the University, nor the names of
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CHRIS PROVENZANO AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL CHRIS PROVENZANO, THE REGENTS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */ 

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
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include "test.h"


static void *
empty(void *arg)
{

	return (void *)0x12345678;
}

static void *
sleeper(void *arg)
{

	pthread_set_name_np(pthread_self(), "slpr");
	sleep(10);
	PANIC("sleeper timed out");
}


int
main(int argc, char *argv[])
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
		/* This fails when stdout is /dev/null!? */
		/*CHECKe*/(fcntl(STDOUT_FILENO, F_SETFL, 
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
		printf("sleeper should have disappeared\n");

		/*
		 * The following is bogus.  The sleeper thread was
		 * freed before the fork returned.   Calling pthread_join
		 * dereferences the 'sleeper_thread' pointer which no
		 * longer points to a valid thread structure.  If the
		 * function returns ESRCH it's only because the freed
		 * memory hasn't been reused yet.
		ASSERT(ESRCH == pthread_join(sleeper_thread, &result));
		printf("sleeper disappeared correctly\n");
		 */

		/* Test starting another thread */
		CHECKr(pthread_create(&sleeper_thread, NULL, empty, NULL));
		sleep(1);
		CHECKr(pthread_join(sleeper_thread, &result));
		ASSERT(result == (void *)0x12345678);
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
