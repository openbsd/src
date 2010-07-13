/*	$OpenBSD: uthread_execve.c,v 1.12 2010/07/13 03:10:29 guenther Exp $	*/
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
 * $FreeBSD: uthread_execve.c,v 1.8 1999/08/28 00:03:30 peter Exp $
 */
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

int 
execve(const char *name, char *const * argv, char *const * envp)
{
	struct pthread	*curthread = _get_curthread();
	int		flags;
	int             i;
	int             ret;
	int		err;
	struct sigaction act;
	struct itimerval itimer;
	struct itimerval oitimer;
	sigset_t	oset;

	/* Disable the interval timer: */
	itimer.it_interval.tv_sec  = 0;
	itimer.it_interval.tv_usec = 0;
	itimer.it_value.tv_sec     = 0;
	itimer.it_value.tv_usec    = 0;
	setitimer(_ITIMER_SCHED_TIMER, &itimer, &oitimer);

	/*
	 * Enter a loop to set all file descriptors to blocking
	 * if they were not created as non-blocking:
	 */
	for (i = 0; i < _thread_max_fdtsize; i++) {
		/* Check if this file descriptor is in use: */
		if (_thread_fd_table[i] != NULL &&
		    _thread_fd_table[i]->status_flags != NULL &&
		    !(_thread_fd_table[i]->status_flags->flags & O_NONBLOCK)) {
			/* Skip if the close-on-exec flag is set */
			flags = _thread_sys_fcntl(i, F_GETFD, NULL);
			if ((flags & FD_CLOEXEC) != 0)
				continue;	/* don't bother, no point */
			/* Get the current flags: */
			flags = _thread_sys_fcntl(i, F_GETFL, NULL);
			/* Clear the nonblocking file descriptor flag: */
			_thread_sys_fcntl(i, F_SETFL, flags & ~O_NONBLOCK);
		}
	}

#define RESET_SIGNAL(sig) \
	(_thread_sigact[(sig) - 1].sa_handler == SIG_IGN && \
	_thread_sys_sigaction((sig), &_thread_sigact[(sig) - 1], NULL) != 0)
	
	/* Reset the behavior for the signals that the library uses */
	if (RESET_SIGNAL(_SCHED_SIGNAL) ||
	    RESET_SIGNAL(SIGINFO) ||
	    RESET_SIGNAL(SIGCHLD))
		PANIC("Cannot reset signal handlers");

	/* Set the signal mask: */
	_thread_sys_sigprocmask(SIG_SETMASK, &curthread->sigmask, &oset);

	/* Execute the process: */
	ret = _thread_sys_execve(name, argv, envp);
	err = errno;

	/* execve failed; try to restore the state the thread library needs */

#define REINIT_SIGNAL(sig) \
	(_thread_sigact[(sig) - 1].sa_handler == SIG_IGN && \
	_thread_sys_sigaction((sig), &act, NULL) != 0)

	_thread_sys_sigprocmask(SIG_SETMASK, &oset, NULL);
	sigfillset(&act.sa_mask);
	act.sa_handler = (void (*) (int)) _thread_sig_handler;
	act.sa_flags = SA_SIGINFO;
	if (REINIT_SIGNAL(_SCHED_SIGNAL) ||
	    REINIT_SIGNAL(SIGINFO) ||
	    REINIT_SIGNAL(SIGCHLD))
		PANIC("Cannot reinitialize signal handlers");
	_thread_nonblock_fds();
	setitimer(_ITIMER_SCHED_TIMER, &oitimer, NULL);

	/* Return the completion status: */
	errno = err;
	return (ret);
}
#endif
