/*	$OpenBSD: uthread_sig.c,v 1.17 2003/01/24 21:03:15 marc Exp $	*/
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
 * $FreeBSD: uthread_sig.c,v 1.20 1999/09/29 15:18:39 marcel Exp $
 */
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

/* Static variables: */
static _spinlock_lock_t	signal_lock = _SPINLOCK_UNLOCKED;
siginfo_t		info_queue[NSIG];
volatile sig_atomic_t	pending_sigs[NSIG];
volatile sig_atomic_t	check_pending = 0;

/* Initialize signal handling facility: */
void
_thread_sig_init(void)
{
	int i;

	/* Clear local state */
	for (i = 1; i < NSIG; i++) {
		pending_sigs[i - 1] = 0;
	}

	/* Clear the lock: */
	signal_lock = _SPINLOCK_UNLOCKED;
}

/*
 * Process a pending signal unless it is already in progress.  If the
 * SA_NODEFER  flag is on, process it any way.
 */
void
_thread_sig_process(int sig, struct sigcontext * scp)
{
	int locked = 0;

	if (_atomic_lock(&signal_lock) == 0)
		locked = 1;

	if (locked || _thread_sigact[sig - 1].sa_flags & SA_NODEFER) {
		pending_sigs[sig - 1] = 0;
		_thread_sig_handle(sig, scp);
	} else
		check_pending = 1;
	if (locked)
		signal_lock = _SPINLOCK_UNLOCKED;
}

/*
 * This is the only installed signal handler.   In addition to handling
 * thread kernel signals it is installed in place of application handlers
 * and dispatches signals appropriately.
 */
void
_thread_sig_handler(int sig, siginfo_t *info, struct sigcontext * scp)
{
	struct pthread	*curthread = _get_curthread();
	char	c;
	int	i;

	/*
	 * save the info for this signal in a per signal queue of depth
	 * one.  Per a POSIX suggestion, only the info for the first
	 * of multiple activations of the same signal is kept.
	 */
	if (pending_sigs[sig - 1] == 0) {
		pending_sigs[sig - 1]++;
		memcpy(&info_queue[sig - 1], info, sizeof *info);
	}

	if (sig == _SCHED_SIGNAL) {
		/* Update the scheduling clock: */
		gettimeofday((struct timeval *)&_sched_tod, NULL);
		_sched_ticks++;

		/* only process signal when scheduler isn't running */
		if (_thread_kern_in_sched == 0) {
			if (curthread->sig_defer_count > 0) {
				/*
				 * The scheduler interrupt has come when
				 * the currently running thread has deferred
				 * thread signals.
				 */
				curthread->yield_on_sig_undefer = 1;
			} else {
				/* Schedule the next thread. */
				_thread_kern_sched(scp);

				/*
				 * The scheduler currently returns here instead
				 * of calling sigreturn due to a sparc sigreturn
				 * bug.   We should also return.   That brings
				 * us back to the sigtramp code which will
				 * sigreturn to the context stored on the
				 * current stack (which is the same as scp,
				 * above). The code originally did this:
				 * 
				 * PANIC("Returned to signal function "
				 *	 "from scheduler");
				 */
				return;
			}
		}
	} else if ((_queue_signals != 0) ||
		   ((_thread_kern_in_sched == 0) &&
		    (curthread->sig_defer_count > 0))) {
		/*
		 * The kernel has been interrupted while the scheduler
		 * is accessing the scheduling queues or there is a currently
		 * running thread that has deferred signals.
		 *
		 * Cast the signal number to a character variable and Write
		 * the signal number to the kernel pipe so that it will be
		 * ready to read when this signal handler returns.
		 */
		c = sig;
		_thread_sys_write(_thread_kern_pipe[1], &c, 1);
		_sigq_check_reqd = 1;
	} else {
		/* process the signal */
		_thread_sig_process(sig, scp);
		/*
		 * process pending signals unless a current signal handler
		 * is running (signal_lock is locked).   When locked
		 * the pending signals will be processed when the running
		 * handler returns.
		 */
		while (check_pending != 0 && _atomic_lock(&signal_lock) == 0) {
			check_pending = 0;
			signal_lock = _SPINLOCK_UNLOCKED;
			for (i = 1; i < NSIG; i++)
				if (pending_sigs[i - 1])
					_thread_sig_process(i, scp);
		}
	}

}

/*
 * Clear the pending flag for the given signal on all threads
 */
void
_clear_pending_flag(int sig)
{
	pthread_t pthread;

	TAILQ_FOREACH(pthread, &_thread_list, tle) {
		sigdelset(&pthread->sigpend, sig);
	}
}


/*
 * Process the given signal.
 */
void
_thread_sig_handle(int sig, struct sigcontext * scp)
{
	struct pthread	*curthread = _get_curthread();
	int		i;
	pthread_t	pthread, pthread_next;

	if (sig == SIGINFO)
		_thread_dump_info();	/* Dump thread information */
	else if (sig == _SCHED_SIGNAL)
		; /* This shouldn't ever occur (should this panic?). */
	else {
		if (sig == SIGCHLD) {
			/*
			 * Go through the file list and set all files
			 * to non-blocking again in case the child
			 * set some of them to block. Sigh.
			 */
			for (i = 0; i < _thread_dtablesize; i++)
				if (_thread_fd_table[i] != NULL)
					_thread_sys_fcntl(i, F_SETFL,
					    _thread_fd_table[i]->flags |
					    O_NONBLOCK);
		}

		/*
		 * POSIX says that pending SIGCONT signals are
		 * discarded when one of these signals occurs.
		 */
		if (sig == SIGTSTP || sig == SIGTTIN || sig == SIGTTOU)
			_clear_pending_flag(SIGCONT);

		/*
		 * Enter a loop to process each thread in the waiting
		 * list that is sigwait-ing on a signal.  Since POSIX
		 * doesn't specify which thread will get the signal
		 * if there are multiple waiters, we'll give it to the
		 * first one we find.
		 */
		for (pthread = TAILQ_FIRST(&_waitingq);
		     pthread != NULL; pthread = pthread_next) {
			/*
			 * Grab the next thread before possibly destroying
			 * the link entry.
			 */
			pthread_next = TAILQ_NEXT(pthread, pqe);

			if ((pthread->state == PS_SIGWAIT) &&
			    sigismember(pthread->data.sigwait, sig)) {
				/* Change the state of the thread to run: */
				PTHREAD_NEW_STATE(pthread,PS_RUNNING);

				/* Return the signal number: */
				pthread->signo = sig;

				/*
				 * Do not attempt to deliver this signal
				 * to other threads.
				 */
				return;
			}
		}

		if (_thread_sigact[sig - 1].sa_handler != SIG_IGN) {
			/*
			 * mark the signal as pending for each thread
			 * and give the thread a chance to update
			 * its state.
			 */
			TAILQ_FOREACH(pthread, &_thread_list, tle) {
				/* Current thread inside critical region? */
				if (curthread->sig_defer_count > 0)
					pthread->sig_defer_count++;
				_thread_signal(pthread,sig);
				if (curthread->sig_defer_count > 0)
					pthread->sig_defer_count--;
			}
			/*
			 * Give the current thread a chance to dispatch
			 * the signals.  Other threads will get thier
			 * chance (if the signal is still pending) later.
			 */
			_dispatch_signals(scp);

		}
	}
}

/* Perform thread specific actions in response to a signal: */
void
_thread_signal(pthread_t pthread, int sig)
{
	/*
	 * Flag the signal as pending. It may be dispatched later.
	 */
	sigaddset(&pthread->sigpend,sig);

	/* skip this thread if signal is masked */
	if (sigismember(&pthread->sigmask, sig))
		return;

	/*
	 * Process according to thread state:
	 */
	switch (pthread->state) {
	/*
	 * States which do not change when a signal is trapped:
	 */
	case PS_COND_WAIT:
	case PS_DEAD:
	case PS_FDLR_WAIT:
	case PS_FDLW_WAIT:
	case PS_FILE_WAIT:
	case PS_JOIN:
	case PS_MUTEX_WAIT:
	case PS_RUNNING:
	case PS_SIGTHREAD:
	case PS_SIGWAIT:
	case PS_SUSPENDED:
	case PS_SPINBLOCK:
	case PS_DEADLOCK:
	case PS_STATE_MAX: /* only here to quell a compiler warning */
		/* Nothing to do here. */
		break;

	/*
	 * The wait state is a special case due to the handling of
	 * SIGCHLD signals.
	 */
	case PS_WAIT_WAIT:
		/*
		 * Check for signals other than the death of a child
		 * process:
		 */
		if (sig != SIGCHLD)
			/* Flag the operation as interrupted: */
			pthread->interrupted = 1;

		/* Change the state of the thread to run: */
		PTHREAD_NEW_STATE(pthread,PS_RUNNING);

		/* Return the signal number: */
		pthread->signo = sig;
		break;

	/*
	 * States that are interrupted by the occurrence of a signal
	 * other than the scheduling alarm:
	 */
	case PS_FDR_WAIT:
	case PS_FDW_WAIT:
	case PS_POLL_WAIT:
	case PS_SLEEP_WAIT:
	case PS_SELECT_WAIT:
		if (sig != SIGCHLD ||
		    _thread_sigact[sig - 1].sa_handler != SIG_DFL) {
			/* Flag the operation as interrupted: */
			pthread->interrupted = 1;

			if (pthread->flags & PTHREAD_FLAGS_IN_WORKQ)
				PTHREAD_WORKQ_REMOVE(pthread);

			/* Change the state of the thread to run: */
			PTHREAD_NEW_STATE(pthread,PS_RUNNING);

			/* Return the signal number: */
			pthread->signo = sig;
		}
		break;

	case PS_SIGSUSPEND:
		/*
		 * Only wake up the thread if the signal is unblocked
		 * and there is a handler installed for the signal.
		 */
		if (_thread_sigact[sig - 1].sa_handler != SIG_DFL) {
			/* Change the state of the thread to run: */
			PTHREAD_NEW_STATE(pthread,PS_RUNNING);

			/* Return the signal number: */
			pthread->signo = sig;
		}
		break;
	}
}

/*
 * possibly dispatch a signal to the current thread.
 */
void
_dispatch_signals(struct sigcontext * scp)
{
	struct pthread	*curthread = _get_curthread();
	struct sigaction act;
	void (*action)(int, siginfo_t *, void *);
	int i;

	/*
	 * Check if there are pending signals for the running
	 * thread that aren't blocked:
	 */
	if ((curthread->sigpend & ~curthread->sigmask) != 0)
		/* Look for all possible pending signals: */
		for (i = 1; i < NSIG; i++)
			/*
			 * Check that a custom handler is installed
			 * and if the signal is not blocked:
			 */
			if (_thread_sigact[i - 1].sa_handler != SIG_DFL &&
			    _thread_sigact[i - 1].sa_handler != SIG_IGN &&
			    sigismember(&curthread->sigpend,i) &&
			    !sigismember(&curthread->sigmask,i)) {
				action = _thread_sigact[i - 1].sa_sigaction;
				_clear_pending_flag(i);

				/* clear custom handler if SA_RESETHAND set. */
				if (_thread_sigact[i - 1].sa_flags &
				    SA_RESETHAND) {
					act.sa_handler = SIG_DFL;
					act.sa_flags = 0;
					sigemptyset(&act.sa_mask);
					sigaction(i, &act, NULL);
				}

				/*
				 * Dispatch the signal via the custom signal
				 * handler.
				 */
				(*action)(i, &info_queue[i - 1], scp);
			}
}
#endif
