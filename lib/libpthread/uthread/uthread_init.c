/*	$OpenBSD: uthread_init.c,v 1.27 2003/01/31 04:46:17 marc Exp $	*/
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
	&_thread_fd_lock,
	&_thread_fd_unlock,
	&_thread_malloc_init,
	&_thread_malloc_lock,
	&_thread_malloc_unlock,
	&_libc_private_storage,
	&_libc_private_storage_lock,
	&_libc_private_storage_unlock,
	&flockfile,
	&_flockfile_debug,
	&ftrylockfile,
	&funlockfile
};

/*
 * Threaded process initialization
 */
void
_thread_init(void)
{
	int		fd;
	int             flags;
	int             i;
	size_t		len;
	int		mib[2];
	struct clockinfo clockinfo;
	struct sigaction act;

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
	if (_thread_sys_pipe(_thread_kern_pipe) != 0) {
		/* Cannot create pipe, so abort: */
		PANIC("Cannot create kernel pipe");
	}
	/* Get the flags for the read pipe: */
	else if ((flags = _thread_sys_fcntl(_thread_kern_pipe[0], F_GETFL, NULL)) == -1) {
		/* Abort this application: */
		PANIC("Cannot get kernel read pipe flags");
	}
	/* Make the read pipe non-blocking: */
	else if (_thread_sys_fcntl(_thread_kern_pipe[0], F_SETFL, flags | O_NONBLOCK) == -1) {
		/* Abort this application: */
		PANIC("Cannot make kernel read pipe non-blocking");
	}
	/* Get the flags for the write pipe: */
	else if ((flags = _thread_sys_fcntl(_thread_kern_pipe[1], F_GETFL, NULL)) == -1) {
		/* Abort this application: */
		PANIC("Cannot get kernel write pipe flags");
	}
	/* Make the write pipe non-blocking: */
	else if (_thread_sys_fcntl(_thread_kern_pipe[1], F_SETFL, flags | O_NONBLOCK) == -1) {
		/* Abort this application: */
		PANIC("Cannot get kernel write pipe flags");
	}
	/* Allocate and initialize the ready queue: */
	else if (_pq_alloc(&_readyq, PTHREAD_MIN_PRIORITY, PTHREAD_LAST_PRIORITY) != 0) {
		/* Abort this application: */
		PANIC("Cannot allocate priority ready queue.");
	}
	/* Allocate memory for the thread structure of the initial thread: */
	else if ((_thread_initial = (pthread_t) malloc(sizeof(struct pthread))) == NULL) {
		/*
		 * Insufficient memory to initialise this application, so
		 * abort:
		 */
		PANIC("Cannot allocate memory for initial thread");
	} else {
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
		pthread_set_name_np(_thread_initial, (char *)"main");

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
			if (i == SIGKILL || i == SIGSTOP) {
			}

			/* Get the signal handler details: */
			else if (_thread_sys_sigaction(i, NULL,
			    &_thread_sigact[i - 1]) != 0) {
				/*
				 * Abort this process if signal
				 * initialisation fails:
				 */
				PANIC("Cannot read signal handler info");
			}

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
		    _thread_sys_sigaction(SIGCHLD,       &act, NULL) != 0) {
			/*
			 * Abort this process if signal initialization fails:
			 */
			PANIC("Cannot initialize signal handler");
		}
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
		if ((_thread_dtablesize = getdtablesize()) < 0) {
			/*
			 * Cannot get the system defined table size, so abort
			 * this process.
			 */
			PANIC("Cannot get dtablesize");
		}
		/* Allocate memory for the file descriptor table: */
		if ((_thread_fd_table = (struct fd_table_entry **) malloc(sizeof(struct fd_table_entry *) * _thread_dtablesize)) == NULL) {
			/* Avoid accesses to file descriptor table on exit: */
			_thread_dtablesize = 0;

			/*
			 * Cannot allocate memory for the file descriptor
			 * table, so abort this process.
			 */
			PANIC("Cannot allocate memory for file descriptor table");
		}
		/* Allocate memory for the pollfd table: */
		if ((_thread_pfd_table = (struct pollfd *) malloc(sizeof(struct pollfd) * _thread_dtablesize)) == NULL) {
			/*
			 * Cannot allocate memory for the file descriptor
			 * table, so abort this process.
			 */
			PANIC("Cannot allocate memory for pollfd table");
		} else {
			/*
			 * Enter a loop to initialise the file descriptor
			 * table:
			 */
			for (i = 0; i < _thread_dtablesize; i++) {
				/* Initialise the file descriptor table: */
				_thread_fd_table[i] = NULL;
			}

			/* Initialize stdio file descriptor table entries: */
			for (i = 0; i < 3; i++) {
				if ((_thread_fd_table_init(i) != 0) &&
				    (errno != EBADF))
					PANIC("Cannot initialize stdio file "
					    "descriptor table entry");
			}
		}
	}

	/* Initialise the garbage collector mutex and condition variable. */
	if (pthread_mutex_init(&_gc_mutex,NULL) != 0 ||
	    pthread_cond_init(&_gc_cond,NULL) != 0)
		PANIC("Failed to initialise garbage collector mutex or condvar");

	_thread_autoinit_dummy_decl = 0;
}
#endif
