/* $OpenBSD: uthread_kill.c,v 1.10 2003/01/31 04:46:17 marc Exp $ */
/* PUBLIC_DOMAIN <marc@snafu.org> */

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#ifdef _THREAD_SAFE
#include <pthread.h>

#include "pthread_private.h"

/*
 * Fake up a minimal siginfo_t for the given signal unless one is already
 * pending. The signal number is assumed to be valid.
 */
void
_thread_kill_siginfo(int sig)
{
	struct sigstatus *ss = &_thread_sigq[sig - 1];

	_SPINLOCK(&ss->lock);
	if (ss->pending == 0) {
		ss->pending = 1;
		memset(&ss->siginfo, 0, sizeof ss->siginfo);
		ss->siginfo.si_signo = sig;
		ss->siginfo.si_code = SI_USER;
		ss->siginfo.si_errno = errno;
		ss->siginfo.si_pid = getpid();
	}
	_SPINUNLOCK(&ss->lock);
}

/*
 * Validate the signal number and thread.  If valid process the signal.
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
				} else {
					_thread_kill_siginfo(sig);
					_thread_signal(pthread,sig);
				}
			}
		}
	} else
		ret = EINVAL;

	return ret;
}
#endif
