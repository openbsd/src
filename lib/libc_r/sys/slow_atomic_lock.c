/*	$OpenBSD: slow_atomic_lock.c,v 1.1 1998/11/20 11:15:38 d Exp $	*/

#include "pthread_private.h"
#include "spinlock.h"
#include <signal.h>

/*
 * uthread atomic lock: 
 * 	attempt to acquire a lock (by giving it a non-zero value).
 *	Return zero on success, or the lock's value on failure
 *	This uses signal masking to make sure that no other thread
 *	can modify the lock while processing, hence it is very slow.
 */
register_t
_thread_slow_atomic_lock(volatile register_t *lock)
{
	register_t old;
	sigset_t oldset, newset = (sigset_t)~0;

	/* block signals - incurs a context switch */
	if (_thread_sys_sigprocmask(SIG_SETMASK, &newset, &oldset) < 0)
		PANIC("_atomic_lock block");

	old = *lock;
	if (old == 0)
		*lock = 1;

	/* restore signal mask to what it was */
	if (_thread_sys_sigprocmask(SIG_SETMASK, &oldset, NULL) < 0)
		PANIC("_atomic_lock restore");

	return old;
}
