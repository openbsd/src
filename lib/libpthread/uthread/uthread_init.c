/*	$OpenBSD: uthread_init.c,v 1.39 2007/06/05 18:11:49 kurt Exp $	*/
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
 * $FreeBSD: uthread_init.c,v 1.18 1999/08/28 00:03:36 peter Exp $
 */

/* Allocate space for global thread variables here: */
#define GLOBAL_PTHREAD_PRIVATE

#include <sys/types.h>
#include <sys/param.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/ttycom.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <sys/resource.h>

#include <dlfcn.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef _THREAD_SAFE
#include <machine/reg.h>
#include <pthread.h>
#include "pthread_private.h"

/* Global thread variables. */
_stack_list_t	_stackq;

extern int _thread_autoinit_dummy_decl;

/*
 * All weak references used within libc that are redefined in libpthread
 * MUST be in this table.   This is necessary to force the proper version to
 * be used when linking -static.
 */
static void *references[] = {
	&_exit,
	&accept,
	&bind,
	&close,
	&closefrom,
	&connect,
	&dup,
	&dup2,
	&execve,
	&fchflags,
	&fchmod,
	&fchown,
	&fcntl,
	&flock,
	&fork,
	&fpathconf,
	&fstat,
	&fstatfs,
	&fsync,
	&getdirentries,
	&getpeername,
	&getsockname,
	&getsockopt,
	&ioctl,
	&kevent,
	&kqueue,
	&listen,
	&msync,
	&nanosleep,
	&open,
	&pipe,
	&poll,
	&read,
	&readv,
	&recvfrom,
	&recvmsg,
	&select,
	&sendmsg,
	&sendto,
	&setsockopt,
	&shutdown,
	&sigaction,
	&sigaltstack,
	&sigpending,
	&sigprocmask,
	&sigsuspend,
	&socket,
	&socketpair,
	&vfork,
	&wait4,
	&write,
	&writev,
	/* libc thread-safe helper functions */
	&_thread_malloc_init,
	&_thread_malloc_lock,
	&_thread_malloc_unlock,
	&_thread_atexit_lock,
	&_thread_atexit_unlock,
	&_thread_tag_lock,
	&_thread_tag_unlock,
	&_thread_tag_storage,
	&_thread_mutex_lock,
	&_thread_mutex_unlock,
	&_thread_mutex_destroy,
	&flockfile,
	&ftruncate,
	&ftrylockfile,
	&funlockfile,
	&lseek
};

/*
 * Threaded process initialization
 */
void
_thread_init(void)
{
	int		fd;
	int             flags;
	int		res;
	int             i;
	size_t		len;
	int		mib[2];
	struct clockinfo clockinfo;
	struct sigaction act;
	struct rlimit rl;

	/* Check if this function has already been called: */
	if (_thread_initial)
		/* Only initialise the threaded application once. */
		return;

	if (references[0] == NULL)
		PANIC("Failed loading mandatory references in _thread_init");

	/*
	 * Check for the special case of this process running as
	 * or in place of init as pid = 1:
	 */
	if (getpid() == 1) {
		/*
		 * Setup a new session for this process which is
		 * assumed to be running as root.
		 */
		if (setsid() == -1)
			PANIC("Can't set session ID");
		if (revoke(_PATH_CONSOLE) != 0)
			PANIC("Can't revoke console");
		if ((fd = _thread_sys_open(_PATH_CONSOLE, O_RDWR)) < 0)
			PANIC("Can't open console");
		if (setlogin("root") == -1)
			PANIC("Can't set login to root");
		if (_thread_sys_ioctl(fd,TIOCSCTTY, (char *) NULL) == -1)
			PANIC("Can't set controlling terminal");
		if (_thread_sys_dup2(fd,0) == -1 ||
		    _thread_sys_dup2(fd,1) == -1 ||
		    _thread_sys_dup2(fd,2) == -1)
			PANIC("Can't dup2");
	}

	/*
	 * Create a pipe that is written to by the signal handler to prevent
	 * signals being missed in calls to _select:
	 */
	if (_thread_sys_pipe(_thread_kern_pipe) != 0)
		PANIC("Cannot create kernel pipe");

	flags = _thread_sys_fcntl(_thread_kern_pipe[0], F_GETFL, NULL);
	if (flags == -1)
		PANIC("Cannot get kernel read pipe flags");

	res = _thread_sys_fcntl(_thread_kern_pipe[0], F_SETFL,
			       flags | O_NONBLOCK);
	if (res == -1)
		PANIC("Cannot make kernel read pipe non-blocking");

	flags = _thread_sys_fcntl(_thread_kern_pipe[1], F_GETFL, NULL);
	if (flags == -1)
		PANIC("Cannot get kernel write pipe flags");

	res = _thread_sys_fcntl(_thread_kern_pipe[1], F_SETFL,
				flags | O_NONBLOCK);
	if (res == -1)
		PANIC("Cannot make kernel write pipe non-blocking");

	res = _pq_alloc(&_readyq, PTHREAD_MIN_PRIORITY, PTHREAD_LAST_PRIORITY);
	if (res != 0)
		PANIC("Cannot allocate priority ready queue.");

	_thread_initial = (pthread_t) malloc(sizeof(struct pthread));
	if (_thread_initial == NULL)
		PANIC("Cannot allocate memory for initial thread");


	/* Zero the global kernel thread structure: */
	memset(&_thread_kern_thread, 0, sizeof(struct pthread));
	_thread_kern_thread.flags = PTHREAD_FLAGS_PRIVATE;
	memset(_thread_initial, 0, sizeof(struct pthread));

	/* Initialize the waiting and work queues: */
	TAILQ_INIT(&_waitingq);
	TAILQ_INIT(&_workq);

	/* Initialize the scheduling switch hook routine: */
	_sched_switch_hook = NULL;

	/* Initialize the thread stack cache: */
	SLIST_INIT(&_stackq);

	/*
	 * Write a magic value to the thread structure
	 * to help identify valid ones:
	 */
	_thread_initial->magic = PTHREAD_MAGIC;

	/* Set the initial cancel state */
	_thread_initial->cancelflags = PTHREAD_CANCEL_ENABLE |
		PTHREAD_CANCEL_DEFERRED;

	/* Default the priority of the initial thread: */
	_thread_initial->base_priority = PTHREAD_DEFAULT_PRIORITY;
	_thread_initial->active_priority = PTHREAD_DEFAULT_PRIORITY;
	_thread_initial->inherited_priority = 0;

	/* Initialise the state of the initial thread: */
	_thread_initial->state = PS_RUNNING;

	/* Initialize joiner to NULL (no joiner): */
	_thread_initial->joiner = NULL;

	/* Initialize the owned mutex queue and count: */
	TAILQ_INIT(&(_thread_initial->mutexq));
	_thread_initial->priority_mutex_count = 0;

	/* Initialize the global scheduling time: */
	_sched_ticks = 0;
	gettimeofday((struct timeval *) &_sched_tod, NULL);

	/* Initialize last active: */
	_thread_initial->last_active = (long) _sched_ticks;

	/* Give it a useful name */
	pthread_set_name_np(_thread_initial, "main");

	/* Initialise the rest of the fields: */
	_thread_initial->poll_data.nfds = 0;
	_thread_initial->poll_data.fds = NULL;
	_thread_initial->sig_defer_count = 0;
	_thread_initial->slice_usec = -1;
	_thread_initial->sig_saved = 0;
	_thread_initial->yield_on_sig_undefer = 0;
	_thread_initial->specific_data = NULL;
	_thread_initial->cleanup = NULL;
	_thread_initial->flags = 0;
	_thread_initial->error = 0;
	_SPINLOCK_INIT(&_thread_initial->lock);
	TAILQ_INIT(&_thread_list);
	TAILQ_INSERT_HEAD(&_thread_list, _thread_initial, tle);
	_set_curthread(_thread_initial);
	TAILQ_INIT(&_atfork_list);
	pthread_mutex_init(&_atfork_mutex, NULL);

	/* Initialise the global signal action structure: */
	sigfillset(&act.sa_mask);
	act.sa_handler = (void (*) (int)) _thread_sig_handler;
	act.sa_flags = SA_SIGINFO;

	/* Clear pending signals for the process: */
	sigemptyset(&_process_sigpending);

	/* Initialize signal handling: */
	_thread_sig_init();

	/* Enter a loop to get the existing signal status: */
	for (i = 1; i < NSIG; i++) {
		/* Check for signals which cannot be trapped: */
		if (i == SIGKILL || i == SIGSTOP)
			continue;

		/* Get the signal handler details: */
		if (_thread_sys_sigaction(i, NULL, &_thread_sigact[i - 1]) != 0)
			PANIC("Cannot read signal handler info");

		/* Initialize the SIG_DFL dummy handler count. */
		_thread_dfl_count[i] = 0;
	}

	/*
	 * Install the signal handler for the most important
	 * signals that the user-thread kernel needs. Actually
	 * SIGINFO isn't really needed, but it is nice to have.
	 */
	if (_thread_sys_sigaction(_SCHED_SIGNAL, &act, NULL) != 0 ||
	    _thread_sys_sigaction(SIGINFO,       &act, NULL) != 0 ||
	    _thread_sys_sigaction(SIGCHLD,       &act, NULL) != 0)
		PANIC("Cannot initialize signal handler");

	_thread_sigact[_SCHED_SIGNAL - 1].sa_flags = SA_SIGINFO;
	_thread_sigact[SIGINFO - 1].sa_flags = SA_SIGINFO;
	_thread_sigact[SIGCHLD - 1].sa_flags = SA_SIGINFO;

	/* Get the process signal mask: */
	_thread_sys_sigprocmask(SIG_SETMASK, NULL, &_process_sigmask);

	/* Get the kernel clockrate: */
	mib[0] = CTL_KERN;
	mib[1] = KERN_CLOCKRATE;
	len = sizeof (struct clockinfo);
	if (sysctl(mib, 2, &clockinfo, &len, NULL, 0) == 0)
		_clock_res_usec = clockinfo.tick > CLOCK_RES_USEC_MIN ?
			clockinfo.tick : CLOCK_RES_USEC_MIN;

	/* Get the table size: */
	if (getrlimit(RLIMIT_NOFILE, &rl) != 0)
		PANIC("getrlimit failed");

	_thread_init_fdtsize = (int)rl.rlim_cur;
	_thread_max_fdtsize = (int)rl.rlim_max;
	_thread_max_pfdtsize = (nfds_t)rl.rlim_max;

	/* Allocate memory for the file descriptor table: */
	_thread_fd_table = calloc((size_t)_thread_max_fdtsize,
				  sizeof(struct fd_table_entry *));
	if (_thread_fd_table == NULL) {
		_thread_max_fdtsize = 0;
		PANIC("Cannot allocate memory for file descriptor table");
	}

	/* Allocate memory for the pollfd table: */
	_thread_pfd_table = calloc((size_t)_thread_max_pfdtsize, sizeof(struct pollfd));
	if (_thread_pfd_table == NULL)
		PANIC("Cannot allocate memory for pollfd table");

	/* initialize the fd table */
	_thread_fd_init();

	/* Initialise the garbage collector mutex and condition variable. */
	if (pthread_mutex_init(&_gc_mutex,NULL) != 0 ||
	    pthread_cond_init(&_gc_cond,NULL) != 0)
		PANIC("Failed to initialise garbage collector mutex or cond");

#if defined(__ELF__)
	/* Register with dlctl for thread safe dlopen */
	dlctl(NULL, DL_SETTHREADLCK, _thread_kern_lock);
#endif
	_thread_autoinit_dummy_decl = 0;
}
#endif
