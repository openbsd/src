/*	$OpenBSD: uthread_exit.c,v 1.16 2002/10/30 19:11:56 marc Exp $	*/
/*
 * Copyright (c) 1995-1998 John Birrell <jb@cimlogic.com.au>
 * All rights reserved.
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
 *	This product includes software developed by John Birrell.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JOHN BIRRELL AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: uthread_exit.c,v 1.12 1999/08/30 15:45:42 dt Exp $
 */
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

void _exit(int status)
{
	int		flags;
	int             i;
	struct itimerval itimer;

	/* Disable the interval timer: */
	itimer.it_interval.tv_sec  = 0;
	itimer.it_interval.tv_usec = 0;
	itimer.it_value.tv_sec     = 0;
	itimer.it_value.tv_usec    = 0;
	setitimer(_ITIMER_SCHED_TIMER, &itimer, NULL);

	/* Close the pthread kernel pipe: */
	_thread_sys_close(_thread_kern_pipe[0]);
	_thread_sys_close(_thread_kern_pipe[1]);

	/*
	 * Enter a loop to set all file descriptors to blocking
	 * if they were not created as non-blocking:
	 */
	for (i = 0; i < _thread_dtablesize; i++) {
		/* Check if this file descriptor is in use: */
		if (_thread_fd_table[i] != NULL &&
			!(_thread_fd_table[i]->flags & O_NONBLOCK)) {
			/* Get the current flags: */
			flags = _thread_sys_fcntl(i, F_GETFL, NULL);
			/* Clear the nonblocking file descriptor flag: */
			_thread_sys_fcntl(i, F_SETFL, flags & ~O_NONBLOCK);
		}
	}

	/* Call the _exit syscall: */
	_thread_sys__exit(status);
}

static void
numlcat(char *s, int l, size_t sz)
{
	char digit[2];

	/* Inefficient. */
	if (l < 0) {
		l = -l;
		strlcat(s, "-", sz);
	}
	if (l >= 10)
		numlcat(s, l / 10, sz);
	digit[0] = "0123456789"[l % 10];
	digit[1] = '\0';
	strlcat(s, digit, sz);
}

void
_thread_exit(const char *fname, int lineno, const char *string)
{
	char            s[256];

	/* Prepare an error message string: */
	s[0] = '\0';
	strlcat(s, "pid ", sizeof s);
	numlcat(s, (int)_thread_sys_getpid(), sizeof s);
	strlcat(s, ": Fatal error '", sizeof s);
	strlcat(s, string, sizeof s);
	strlcat(s, "' at line ", sizeof s);
	numlcat(s, lineno, sizeof s);
	strlcat(s, " in file ", sizeof s);
	strlcat(s, fname, sizeof s);
	strlcat(s, " (errno = ", sizeof s);
	numlcat(s, errno, sizeof s);
	strlcat(s, ")\n", sizeof s);

	/* Write the string to the standard error file descriptor: */
	_thread_sys_write(2, s, strlen(s));

	/* Force this process to exit: */
	/* XXX - Do we want abort to be conditional on _PTHREADS_INVARIANTS? */
#if defined(_PTHREADS_INVARIANTS)
	{
		struct sigaction sa;
		sigset_t s;

		/* Ignore everything except ABORT */
		sigfillset(&s);
		sigdelset(&s, SIGABRT);
		_thread_sys_sigprocmask(SIG_SETMASK, &s, NULL);

		/* Set the abort handler to default (dump core) */
		sa.sa_handler = SIG_DFL;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = SA_SIGINFO;
		(void)_thread_sys_sigaction(SIGABRT, &sa, NULL);
		(void)_thread_sys_kill(_thread_sys_getpid(), SIGABRT);
		for (;;) ;
	}
#else
	_exit(1);
#endif
}

void
pthread_exit(void *status)
{
	struct pthread	*curthread = _get_curthread();
	pthread_t pthread;

	/* Check if this thread is already in the process of exiting: */
	if ((curthread->flags & PTHREAD_EXITING) != 0) {
		PANIC("Thread has called pthread_exit() from a destructor. POSIX 1003.1 1996 s16.2.5.2 does not allow this!");
	}

	/* Flag this thread as exiting: */
	curthread->flags |= PTHREAD_EXITING;

	/* Save the return value: */
	curthread->ret = status;

	while (curthread->cleanup != NULL) {
		pthread_cleanup_pop(1);
	}
	if (curthread->attr.cleanup_attr != NULL) {
		curthread->attr.cleanup_attr(curthread->attr.arg_attr);
	}
	/* Check if there is thread specific data: */
	if (curthread->specific_data != NULL) {
		/* Run the thread-specific data destructors: */
		_thread_cleanupspecific();
	}

	/*
	 * Lock the garbage collector mutex to ensure that the garbage
	 * collector is not using the dead thread list.
	 */
	if (pthread_mutex_lock(&_gc_mutex) != 0)
		PANIC("Cannot lock gc mutex");

	/* Add this thread to the list of dead threads. */
	TAILQ_INSERT_HEAD(&_dead_list, curthread, dle);

	/*
	 * Signal the garbage collector thread that there is something
	 * to clean up.
	 */
	if (pthread_cond_signal(&_gc_cond) != 0)
		PANIC("Cannot signal gc cond");

	/*
	 * Avoid a race condition where a scheduling signal can occur
	 * causing the garbage collector thread to run.  If this happens,
	 * the current thread can be cleaned out from under us.
	 */
	_thread_kern_sig_defer();

	/* Unlock the garbage collector mutex: */
	if (pthread_mutex_unlock(&_gc_mutex) != 0)
		PANIC("Cannot unlock gc mutex");

	/* Check if there is a thread joining this one: */
	if (curthread->joiner != NULL) {
		pthread = curthread->joiner;
		curthread->joiner = NULL;

		switch (pthread->suspended) {
		case SUSP_JOIN:
			/*
			 * The joining thread is suspended.  Change the
			 * suspension state to make the thread runnable when it
			 * is resumed:
			 */
			pthread->suspended = SUSP_NO;
			break;
		case SUSP_NO:
			/* Make the joining thread runnable: */
			PTHREAD_NEW_STATE(pthread, PS_RUNNING);
			break;
		default:
			PANIC("Unreachable code reached");
		}

		/* Set the return value for the joining thread: */
		pthread->join_status.ret = curthread->ret;
		pthread->join_status.error = 0;
		pthread->join_status.thread = NULL;

		/* Make this thread collectable by the garbage collector. */
		PTHREAD_ASSERT(((curthread->attr.flags & PTHREAD_DETACHED) ==
		    0), "Cannot join a detached thread");
		curthread->attr.flags |= PTHREAD_DETACHED;
	}

	/* Remove this thread from the thread list: */
	TAILQ_REMOVE(&_thread_list, curthread, tle);

	/* This thread will never be re-scheduled. */
	_thread_kern_sched_state(PS_DEAD, __FILE__, __LINE__);

	/* This point should not be reached. */
	PANIC("Dead thread has resumed");
}
#endif
