/* $OpenBSD: uthread_kill.c,v 1.9 2003/01/27 22:22:30 marc Exp $ */
/* PUBLIC_DOMAIN <marc@snafu.org> */

#include <errno.h>
#include <signal.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

/*
 * Validate the signal number and thread.  If valid process the signal.
 * ;;; need to fake up a siginfo_t and put it in the queue for this signal.
 */
int
pthread_kill(pthread_t pthread, int sig)
{
	int ret;

	if (sig >= 0 && sig < NSIG) {
		ret = _find_thread(pthread);
		if (sig != 0) {
			if (_thread_sigact[sig - 1].sa_handler != SIG_IGN) {
				if (pthread->state == PS_SIGWAIT &&
				    sigismember(pthread->data.sigwait, sig)) {
					PTHREAD_NEW_STATE(pthread,PS_RUNNING);
					pthread->signo = sig;
				} else
					_thread_signal(pthread,sig);
			}
		}
	} else
		ret = EINVAL;

	return ret;
}
#endif
