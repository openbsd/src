/* $OpenBSD: uthread_sigaltstack.c,v 1.3 2003/01/20 19:24:24 marc Exp $ */
/* PUBLIC DOMAIN <marc@snafu.org */

#include <signal.h>
#include <errno.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

/*
 * IEEE Std 1003.1-2001 says:
 *
 * "Use of this function by library threads that are not bound to
 * kernel-scheduled entities results in undefined behavior."
 *
 * There exists code (e.g. alpha setjmp) that uses this function
 * to get information about the current stack.
 *
 * The "undefined behaviour" in this implementation is thus:
 * o if ss is *not* null return -1 with errno set to EINVAL
 * o if oss is *not* null fill it in with information about the
 *   current stack and return 0.
 *
 * This lets things like alpha setjmp work in threaded applications.
 */

int
sigaltstack(const struct sigaltstack *ss, struct sigaltstack *oss)
{
	struct pthread *curthread = _get_curthread();

	int ret = 0;
	if (ss != NULL) {
		errno = EINVAL;
		ret = -1;
	} else if (oss != NULL) {
		/*
		 * get the requested info from the kernel if there is no
		 * thread or if the main thread (no thread stack).
		 */
		if (curthread == NULL || curthread->stack == NULL)
			_thread_sys_sigaltstack(ss, oss);
		else {
			oss->ss_sp = curthread->stack->base;
			oss->ss_size = curthread->stack->size;
			oss->ss_flags = SS_DISABLE;
		}
	}
	return (ret);
}
#endif /* _THREAD_SAFE */
