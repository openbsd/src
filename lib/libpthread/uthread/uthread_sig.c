/*	$OpenBSD: uthread_sig.c,v 1.25 2010/01/03 23:05:35 fgsch Exp $	*/
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


/* Initialize signal handling facility: */
void
_thread_sig_init(void)
{
	int i;

	/* Clear local state */
	for (i = 1; i < NSIG; i++) {
		_SPINLOCK_INIT(&_thread_sigq[i - 1].lock);
		_thread_sigq[i - 1].pending = 0;
	}
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
	int	dispatch;
	char	c;

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
	} else {
		/*
		 * save the info for this signal in a per signal queue of depth
		 * one.  Per a POSIX suggestion, only the info for the first
		 * of multiple activations of the same signal is kept.
		 */
		_SPINLOCK(&_thread_sigq[sig - 1].lock);
		if (_thread_sigq[sig - 1].pending == 0) {
			sigaddset(&_process_sigpending, sig);
			_thread_sigq[sig - 1].pending++;
			memcpy(&_thread_sigq[sig - 1].siginfo, info,
			       sizeof *info);
		}
		_SPINUNLOCK(&_thread_sigq[sig - 1].lock);

		if ((_queue_signals != 0) ||
		    ((_thread_kern_in_sched == 0) &&
		     (curthread->sig_defer_count > 0))) {
			/*
			 * The kernel has been interrupted while the scheduler
			 * is accessing the scheduling queues or there is a
			 * currently running thread that has deferred signals.
			 *
			 * Cast the signal number to a character variable
			 * and Write the signal number to the kernel pipe so
			 * that it will be ready to read when this signal
			 * handler returns. 
			 */
			c = (char)sig;
			_thread_sys_write(_thread_kern_pipe[1], &c, 1);
			_sigq_check_reqd = 1;
		} else {
			_queue_signals = 1;
			dispatch = _thread_sig_handle(sig, scp);
			_queue_signals = 0;
			if (dispatch)
				_dispatch_signals(scp);
		}
	}
}

/*
 * Clear the pending flag for the given signal on all threads
 * if per process, or only for the given thread if non null
 * and the signal doesn't exist in _process_sigpending.
 */
void
_thread_clear_pending(int sig, pthread_t thread)
{
	pthread_t pthread;

	_thread_sigq[sig - 1].pending = 0;
	if (sigismember(&_process_sigpending, sig)) {
		sigdelset(&_process_sigpending, sig);
		TAILQ_FOREACH(pthread, &_thread_list, tle) {
			sigdelset(&pthread->sigpend, sig);
		}
	} else if (thread)
		sigdelset(&thread->sigpend, sig);
}


/*
 * Process the given signal.   Returns 1 if the signal may be dispatched,
 * otherwise 0.   Signals MUST be defered when this function is called.
 */
/* ARGSUSED */
int
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
			for (i = 0; i < _thread_max_fdtsize; i++)
				if (_thread_fd_table[i] != NULL &&
				    _thread_fd_table[i]->status_flags != NULL)
					_thread_sys_fcntl(i, F_SETFL,
					    (_thread_fd_table[i]->status_flags->flags & ~_FD_NOTSOCK) |
					    O_NONBLOCK);
		}

		/*
		 * If the handler is SIG_IGN the signal never happened.
		 * remove it from the pending list and return.
		 */
		if (_thread_sigact[sig - 1].sa_handler == SIG_IGN) {
			_thread_clear_pending(sig, curthread);
			return 0;
		}

		/*
		 * POSIX says that pending SIGCONT signals are discarded
		 * when one of these signals occurs and vice versa.
		 */
		if (sig == SIGSTOP || sig == SIGTSTP || sig == SIGTTIN ||
		    sig == SIGTTOU)
			_thread_clear_pending(SIGCONT, curthread);
		if (sig == SIGCONT) {
			_thread_clear_pending(SIGSTOP, curthread);
			_thread_clear_pending(SIGTSTP, curthread);
			_thread_clear_pending(SIGTTIN, curthread);
			_thread_clear_pending(SIGTTOU, curthread);
		}

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
				/*
				 * found a sigwaiter.   Mark its state as
				 * running, save the signal that will be
				 * returned, and mark the signal as no
				 * longer pending. 
				 */
				PTHREAD_NEW_STATE(pthread,PS_RUNNING);
				pthread->signo = sig;
				_thread_clear_pending(sig, pthread);
				return 0;
			}
		}

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
		return 1;
	}
	return 0;
}

/* Perform thread specific actions in response to a signal: */
void
_thread_signal(pthread_t pthread, int sig)
{
	/* Flag the signal as pending. It may be dispatched later. */
	sigaddset(&pthread->sigpend,sig);

	/* skip this thread if signal is masked */
	if (sigismember(&pthread->sigmask, sig))
		return;

	/* Process according to thread state: */
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
 * Dispatch a signal to the current thread after setting up the
 * appropriate signal mask.
 */
void
_dispatch_signal(int sig, struct sigcontext * scp)
{
	struct pthread	*curthread = _get_curthread();
	
	sigset_t set;
	sigset_t oset;
	struct sigaction act;
	void (*action)(int, siginfo_t *, void *);

	/* save off the action and set the signal mask */
	action = _thread_sigact[sig - 1].sa_sigaction;
	set = _thread_sigact[sig - 1].sa_mask;
	if ((_thread_sigact[sig-1].sa_flags & SA_NODEFER) == 0)
		sigaddset(&set, sig);
	oset = curthread->sigmask;
	curthread->sigmask |= set;

	/* clear custom handler if SA_RESETHAND set. */
	if (_thread_sigact[sig - 1].sa_flags & SA_RESETHAND) {
		act.sa_handler = SIG_DFL;
		act.sa_flags = 0;
		sigemptyset(&act.sa_mask);
		sigaction(sig, &act, NULL);
	}

	/*
	 * clear the pending flag, deliver the signal, then reset the
	 * signal mask
	 */
	_thread_clear_pending(sig, curthread);
	(*action)(sig, &_thread_sigq[sig - 1].siginfo, scp);
	curthread->sigmask = oset;
}

/*
 * possibly dispatch a signal to the current thread.
 */
void
_dispatch_signals(struct sigcontext * scp)
{
	struct pthread	*curthread = _get_curthread();
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
			    !sigismember(&curthread->sigmask,i))
				/* dispatch */
				_dispatch_signal(i, scp);
}
#endif
