/*	$OpenBSD: uthread_vfork.c,v 1.5 2008/10/02 23:27:24 deraadt Exp $	*/
/* David Leonard, <d@csee.uq.edu.au>. Public domain. */

#include <unistd.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

pid_t	_dofork(int vfork);

pid_t
vfork(void)
{
	pid_t pid;

	/*
	 * Defer signals to protect the scheduling queues from access
	 * by the signal handler:
	 */
	_thread_kern_sig_defer();

	pid = _dofork(1);

	/*
	 * Undefer and handle pending signals, yielding if necessary:
	 */
	_thread_kern_sig_undefer();

	return (pid);
}
#endif
